#include "lexus_head_unit/hardware/elm327_protocol.h"

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/command_allowlist.h"
#include "lexus_head_unit/hardware/hex.h"
#include "lexus_head_unit/service/clock.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr char promptCharacter = '>';
constexpr std::size_t readChunkBytes = 64;
constexpr std::string_view searchingText = "SEARCHING...";
constexpr std::string_view mode01ResponsePrefix = "41";
constexpr std::string_view negativeResponsePrefix = "7F";
constexpr std::size_t prefixAndPidLength = 4;
constexpr std::size_t maximumDrainReads = 256;

std::string upperCasedWithoutSpaces(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        if (character == ' ' || character == '\t') {
            continue;
        }
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
    }
    return result;
}

bool isNamedError(const std::string& line, Elm327ReplyKind& kind) {
    if (line == "NODATA") {
        kind = Elm327ReplyKind::NoData;
    } else if (line == "UNABLETOCONNECT") {
        kind = Elm327ReplyKind::UnableToConnect;
    } else if (line == "CANERROR") {
        kind = Elm327ReplyKind::CanError;
    } else if (line == "BUFFERFULL") {
        kind = Elm327ReplyKind::BufferFull;
    } else if (line == "STOPPED") {
        kind = Elm327ReplyKind::Stopped;
    } else if (line.rfind("BUSINIT", 0) == 0 && line.find("ERROR") != std::string::npos) {
        kind = Elm327ReplyKind::BusInitError;
    } else if (line == "ERROR" || line.rfind("BUSERROR", 0) == 0 ||
               line.rfind("DATAERROR", 0) == 0 || line == "<DATAERROR" || line == "FBERROR" ||
               line == "ACTALERT" || line == "LVRESET") {
        kind = Elm327ReplyKind::AdapterError;
    } else if (line == "?") {
        kind = Elm327ReplyKind::UnknownCommand;
    } else {
        return false;
    }
    return true;
}

bool isPlainText(const std::string& line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char character) {
        return std::isprint(character) != 0;
    });
}

bool isEvenLengthHex(const std::string& line) {
    return isHexText(line) && line.size() % 2 == 0;
}

// ISO 15765 multi-frame as the adapter prints it with headers off: a 1 to 3 digit length line,
// then "0:", "1:" ... frame lines of even-length hexadecimal.
bool isMultiFrameReply(const std::vector<std::string>& lines) {
    constexpr std::size_t maximumLengthDigits = 3;
    if (lines.size() < 2 || lines.front().empty() || lines.front().size() > maximumLengthDigits ||
        !isHexText(lines.front())) {
        return false;
    }
    return std::all_of(lines.begin() + 1, lines.end(), [](const std::string& line) {
        const std::size_t colon = line.find(':');
        return colon != std::string::npos && colon > 0 && colon <= 2 &&
               isHexText(std::string_view(line).substr(0, colon)) &&
               isEvenLengthHex(line.substr(colon + 1));
    });
}

} // namespace

std::vector<std::string> cleanReplyLines(std::string_view rawText, std::string_view command) {
    std::vector<std::string> lines;
    std::string current;
    const std::string normalisedCommand = upperCasedWithoutSpaces(command);
    auto flush = [&lines, &current, &normalisedCommand]() {
        const std::string cleaned = upperCasedWithoutSpaces(current);
        current.clear();
        if (cleaned.empty() || cleaned == searchingText) {
            return;
        }
        if (lines.empty() && cleaned == normalisedCommand) {
            return;
        }
        lines.push_back(cleaned);
    };
    for (const char character : rawText) {
        if (character == '\r' || character == '\n') {
            flush();
        } else if (character != promptCharacter) {
            current.push_back(character);
        }
    }
    flush();
    return lines;
}

bool isObdRequestCommand(std::string_view command) {
    const std::string normalised = upperCasedWithoutSpaces(command);
    return normalised.rfind("AT", 0) != 0;
}

Elm327ReplyKind classifyReplyLines(const std::vector<std::string>& lines, bool isObdRequest) {
    if (lines.empty()) {
        return Elm327ReplyKind::Malformed;
    }
    for (const std::string& line : lines) {
        Elm327ReplyKind namedKind = Elm327ReplyKind::Malformed;
        if (isNamedError(line, namedKind)) {
            return namedKind;
        }
    }
    const std::string& first = lines.front();
    if (first.rfind(negativeResponsePrefix, 0) == 0 && isHexText(first)) {
        return Elm327ReplyKind::NegativeResponse;
    }
    if (std::all_of(lines.begin(), lines.end(), isEvenLengthHex) || isMultiFrameReply(lines)) {
        return Elm327ReplyKind::Data;
    }
    if (isObdRequest) {
        return Elm327ReplyKind::Malformed;
    }
    if (first == "OK") {
        return Elm327ReplyKind::Ok;
    }
    if (std::all_of(lines.begin(), lines.end(), isPlainText)) {
        return Elm327ReplyKind::Text;
    }
    return Elm327ReplyKind::Malformed;
}

std::optional<std::vector<std::uint8_t>> mode01DataBytes(const Elm327Reply& reply,
                                                         std::uint8_t pid) {
    if (reply.kind != Elm327ReplyKind::Data) {
        return std::nullopt;
    }
    const std::string expectedPrefix = std::string(mode01ResponsePrefix) + hexText(pid);
    for (const std::string& line : reply.lines) {
        if (line.rfind(expectedPrefix, 0) == 0) {
            return hexToBytes(std::string_view(line).substr(prefixAndPidLength));
        }
    }
    return std::nullopt;
}

Elm327Protocol::Elm327Protocol(ByteTransport& transport,
                               const Clock& clock,
                               std::int64_t replyTimeoutMilliseconds)
    : m_transport(&transport),
      m_clock(&clock),
      m_replyTimeoutMilliseconds(replyTimeoutMilliseconds) {}

Elm327Reply Elm327Protocol::execute(std::string_view command) {
    Elm327Reply reply;
    if (!CommandAllowlist::isAllowed(command)) {
        ++m_refusedCommandCount;
        reply.kind = Elm327ReplyKind::Refused;
        return reply;
    }
    if (!m_transport->isOpen()) {
        ++m_linkErrorCount;
        reply.kind = Elm327ReplyKind::LinkError;
        return reply;
    }
    drainStaleBytes();
    std::vector<std::uint8_t> request;
    request.reserve(command.size() + 1);
    for (const char character : command) {
        request.push_back(static_cast<std::uint8_t>(character));
    }
    request.push_back(static_cast<std::uint8_t>('\r'));
    if (m_transport->write(request) != request.size()) {
        ++m_linkErrorCount;
        reply.kind = Elm327ReplyKind::LinkError;
        return reply;
    }
    ++m_commandsSent;
    return readReply(command);
}

// Anything the adapter sent before this command (a late reply after a timeout) is stale.
void Elm327Protocol::drainStaleBytes() {
    std::vector<std::uint8_t> chunk;
    for (std::size_t attempt = 0; attempt < maximumDrainReads; ++attempt) {
        const ReadResult result = m_transport->read(chunk, readChunkBytes, 0);
        if (result.status != ReadStatus::Ok || result.byteCount == 0) {
            return;
        }
        m_discardedStaleByteCount += result.byteCount;
    }
}

Elm327Reply Elm327Protocol::readReply(std::string_view command) {
    Elm327Reply reply;
    const std::int64_t deadline = m_clock->nowMilliseconds() + m_replyTimeoutMilliseconds;
    std::vector<std::uint8_t> chunk;
    bool promptSeen = false;
    while (!promptSeen) {
        const std::int64_t remaining = deadline - m_clock->nowMilliseconds();
        if (remaining <= 0) {
            ++m_timeoutCount;
            reply.kind = Elm327ReplyKind::Timeout;
            return reply;
        }
        const ReadResult result = m_transport->read(chunk, readChunkBytes, remaining);
        if (result.status == ReadStatus::Closed || result.status == ReadStatus::Error) {
            ++m_linkErrorCount;
            reply.kind = Elm327ReplyKind::LinkError;
            return reply;
        }
        if (result.status == ReadStatus::Timeout) {
            ++m_timeoutCount;
            reply.kind = Elm327ReplyKind::Timeout;
            return reply;
        }
        for (std::size_t index = 0; index < result.byteCount && index < chunk.size(); ++index) {
            const char character = static_cast<char>(chunk.at(index));
            reply.rawText.push_back(character);
            if (character == promptCharacter) {
                promptSeen = true;
                break;
            }
        }
        if (reply.rawText.size() > maximumReplyBytes) {
            ++m_malformedReplyCount;
            reply.kind = Elm327ReplyKind::Malformed;
            return reply;
        }
    }
    reply.lines = cleanReplyLines(reply.rawText, command);
    reply.kind = classifyReplyLines(reply.lines, isObdRequestCommand(command));
    if (reply.kind == Elm327ReplyKind::Malformed) {
        ++m_malformedReplyCount;
    }
    return reply;
}

std::uint64_t Elm327Protocol::commandsSent() const {
    return m_commandsSent;
}

std::uint64_t Elm327Protocol::refusedCommandCount() const {
    return m_refusedCommandCount;
}

std::uint64_t Elm327Protocol::timeoutCount() const {
    return m_timeoutCount;
}

std::uint64_t Elm327Protocol::linkErrorCount() const {
    return m_linkErrorCount;
}

std::uint64_t Elm327Protocol::malformedReplyCount() const {
    return m_malformedReplyCount;
}

std::uint64_t Elm327Protocol::discardedStaleByteCount() const {
    return m_discardedStaleByteCount;
}

std::int64_t Elm327Protocol::replyTimeoutMilliseconds() const {
    return m_replyTimeoutMilliseconds;
}

std::string_view toString(Elm327ReplyKind kind) {
    switch (kind) {
    case Elm327ReplyKind::Data:
        return "Data";
    case Elm327ReplyKind::Ok:
        return "Ok";
    case Elm327ReplyKind::Text:
        return "Text";
    case Elm327ReplyKind::NoData:
        return "NoData";
    case Elm327ReplyKind::UnableToConnect:
        return "UnableToConnect";
    case Elm327ReplyKind::CanError:
        return "CanError";
    case Elm327ReplyKind::BufferFull:
        return "BufferFull";
    case Elm327ReplyKind::Stopped:
        return "Stopped";
    case Elm327ReplyKind::BusInitError:
        return "BusInitError";
    case Elm327ReplyKind::AdapterError:
        return "AdapterError";
    case Elm327ReplyKind::UnknownCommand:
        return "UnknownCommand";
    case Elm327ReplyKind::NegativeResponse:
        return "NegativeResponse";
    case Elm327ReplyKind::Malformed:
        return "Malformed";
    case Elm327ReplyKind::Timeout:
        return "Timeout";
    case Elm327ReplyKind::LinkError:
        return "LinkError";
    case Elm327ReplyKind::Refused:
        return "Refused";
    }
    return "UnknownKind";
}

} // namespace lexus_head_unit
