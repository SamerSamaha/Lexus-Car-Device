#pragma once

#include <QByteArray>

#include <cstddef>

namespace lexus_head_unit {

// One HTTP/1.1 request as the phone link needs it (DN-053): method, path, body.
struct HttpRequest {
    QByteArray method;
    QByteArray path;
    QByteArray body;
};

// Collects the bytes of one request as they arrive on a connection and says when it is
// complete. Only what the phone page sends is accepted: a request line, header lines, and a
// body of Content-Length bytes (no chunked transfer). Limits keep a stray client from holding
// memory: the head (request line and headers) at most maximumHeadBytes, the body at most
// maximumBodyBytes.
class HttpRequestParser {
public:
    enum class Result {
        Incomplete,
        Complete,
        Invalid,
        TooLarge,
    };

    static constexpr std::size_t maximumHeadBytes = 8192;
    static constexpr std::size_t maximumBodyBytes = 4096;

    // Appends the bytes and parses as far as possible. Once Complete, Invalid or TooLarge is
    // returned, the result does not change and further bytes are ignored.
    Result feed(const QByteArray& bytes);
    [[nodiscard]] Result result() const;
    // The request; meaningful only after Complete.
    [[nodiscard]] const HttpRequest& request() const;

private:
    Result parseHead(qsizetype headEnd);

    QByteArray m_buffer;
    HttpRequest m_request;
    Result m_result = Result::Incomplete;
    bool m_headParsed = false;
    qsizetype m_bodyStart = 0;
    qsizetype m_contentLength = 0;
};

} // namespace lexus_head_unit
