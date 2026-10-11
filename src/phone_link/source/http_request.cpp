#include "lexus_head_unit/phone_link/http_request.h"

#include <QByteArray>
#include <QList>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr const char* headTerminator = "\r\n\r\n";
constexpr qsizetype headTerminatorLength = 4;
constexpr qsizetype requestLineParts = 3;

} // namespace

HttpRequestParser::Result HttpRequestParser::feed(const QByteArray& bytes) {
    if (m_result != Result::Incomplete) {
        return m_result;
    }
    m_buffer.append(bytes);
    if (!m_headParsed) {
        const qsizetype headEnd = m_buffer.indexOf(headTerminator);
        if (headEnd < 0) {
            if (static_cast<std::size_t>(m_buffer.size()) > maximumHeadBytes) {
                m_result = Result::TooLarge;
            }
            return m_result;
        }
        if (static_cast<std::size_t>(headEnd) > maximumHeadBytes) {
            m_result = Result::TooLarge;
            return m_result;
        }
        m_result = parseHead(headEnd);
        if (m_result != Result::Incomplete) {
            return m_result;
        }
    }
    if (m_buffer.size() - m_bodyStart >= m_contentLength) {
        m_request.body = m_buffer.mid(m_bodyStart, m_contentLength);
        m_result = Result::Complete;
    }
    return m_result;
}

HttpRequestParser::Result HttpRequestParser::parseHead(qsizetype headEnd) {
    m_headParsed = true;
    m_bodyStart = headEnd + headTerminatorLength;
    const QList<QByteArray> lines = m_buffer.left(headEnd).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() != requestLineParts || !requestLine.at(1).startsWith('/') ||
        !requestLine.at(2).startsWith("HTTP/1.")) {
        return Result::Invalid;
    }
    m_request.method = requestLine.at(0);
    m_request.path = requestLine.at(1);
    for (qsizetype index = 1; index < lines.size(); ++index) {
        const QByteArray line = lines.at(index).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) {
            return Result::Invalid;
        }
        const QByteArray name = line.left(colon).trimmed().toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (name == "transfer-encoding") {
            return Result::Invalid;
        }
        if (name == "content-length") {
            bool numeric = false;
            const qlonglong length = value.toLongLong(&numeric);
            if (!numeric || length < 0) {
                return Result::Invalid;
            }
            if (static_cast<unsigned long long>(length) > maximumBodyBytes) {
                return Result::TooLarge;
            }
            m_contentLength = static_cast<qsizetype>(length);
        }
    }
    return Result::Incomplete;
}

HttpRequestParser::Result HttpRequestParser::result() const {
    return m_result;
}

const HttpRequest& HttpRequestParser::request() const {
    return m_request;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
