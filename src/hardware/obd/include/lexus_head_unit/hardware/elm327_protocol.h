#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/service/clock.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

enum class Elm327ReplyKind {
    Data,
    Ok,
    Text,
    NoData,
    UnableToConnect,
    CanError,
    BufferFull,
    Stopped,
    BusInitError,
    AdapterError,
    UnknownCommand,
    NegativeResponse,
    Malformed,
    Timeout,
    LinkError,
    Refused,
};

struct Elm327Reply {
    Elm327ReplyKind kind = Elm327ReplyKind::Malformed;
    std::vector<std::string> lines;
    std::string rawText;
};

constexpr std::int64_t defaultReplyTimeoutMilliseconds = 2000;
constexpr std::size_t maximumReplyBytes = 4096;

// Splits raw adapter text into cleaned lines: "\r" and "\n" separate lines, spaces are removed,
// letters are upper-cased, empty lines, the prompt and "SEARCHING..." are dropped, and a first
// line equal to the echoed command is dropped.
std::vector<std::string> cleanReplyLines(std::string_view rawText, std::string_view command);

// Classifies cleaned lines. For an OBD request only Data, the named errors, NegativeResponse
// and Malformed are possible; Ok and Text exist only for AT commands. Never throws.
Elm327ReplyKind classifyReplyLines(const std::vector<std::string>& lines, bool isObdRequest);

// True for a command that is not an AT command.
bool isObdRequestCommand(std::string_view command);

// The data bytes after "41" and the PID on the first matching line of a Data reply.
std::optional<std::vector<std::uint8_t>> mode01DataBytes(const Elm327Reply& reply,
                                                         std::uint8_t pid);

class Elm327Protocol {
public:
    Elm327Protocol(ByteTransport& transport,
                   const Clock& clock,
                   std::int64_t replyTimeoutMilliseconds = defaultReplyTimeoutMilliseconds);

    Elm327Reply execute(std::string_view command);

    [[nodiscard]] std::uint64_t commandsSent() const;
    [[nodiscard]] std::uint64_t refusedCommandCount() const;
    [[nodiscard]] std::uint64_t timeoutCount() const;
    [[nodiscard]] std::uint64_t linkErrorCount() const;
    [[nodiscard]] std::uint64_t malformedReplyCount() const;
    [[nodiscard]] std::uint64_t discardedStaleByteCount() const;
    [[nodiscard]] std::int64_t replyTimeoutMilliseconds() const;

private:
    void drainStaleBytes();
    Elm327Reply readReply(std::string_view command);

    ByteTransport* m_transport;
    const Clock* m_clock;
    std::int64_t m_replyTimeoutMilliseconds;
    std::uint64_t m_commandsSent = 0;
    std::uint64_t m_refusedCommandCount = 0;
    std::uint64_t m_timeoutCount = 0;
    std::uint64_t m_linkErrorCount = 0;
    std::uint64_t m_malformedReplyCount = 0;
    std::uint64_t m_discardedStaleByteCount = 0;
};

std::string_view toString(Elm327ReplyKind kind);

} // namespace lexus_head_unit
