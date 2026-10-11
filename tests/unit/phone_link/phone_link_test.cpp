// Verifies: REQ-029

#include "lexus_head_unit/phone_link/http_request.h"
#include "lexus_head_unit/phone_link/phone_link_server.h"
#include "lexus_head_unit/phone_link/phone_position.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QByteArray>
#include <QString>

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <vector>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::HttpRequest;
using lexus_head_unit::HttpRequestParser;
using lexus_head_unit::parsePhonePosition;
using lexus_head_unit::PhoneLinkServer;
using lexus_head_unit::PhonePosition;
using lexus_head_unit::PhonePositionTracker;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::Unit;

std::optional<PhonePosition> parsed(const char* body) {
    QString error;
    return parsePhonePosition(QByteArray(body), error);
}

TEST(PhonePositionTest, AValidReportIsParsedAndExtraFieldsAreIgnored) {
    const std::optional<PhonePosition> position =
        parsed(R"({"latitude": 42.2411, "longitude": -83.6130, "accuracy": 6.5, "speed": null})");
    ASSERT_TRUE(position.has_value());
    const PhonePosition value = position.value_or(PhonePosition{});
    EXPECT_DOUBLE_EQ(value.latitudeDegrees, 42.2411);
    EXPECT_DOUBLE_EQ(value.longitudeDegrees, -83.6130);
    EXPECT_DOUBLE_EQ(value.accuracyMetres, 6.5);
    EXPECT_TRUE(parsed(R"({"latitude": 90, "longitude": -180, "accuracy": 0})").has_value());
    EXPECT_TRUE(parsed(R"({"latitude": -90, "longitude": 180, "accuracy": 100000})").has_value());
}

TEST(PhonePositionTest, EveryMalformedReportIsRefusedWithAReason) {
    const std::vector<const char*> bodies = {
        "",
        "not json",
        "[1, 2, 3]",
        R"({"latitude": 42.0, "longitude": -83.0})",
        R"({"latitude": "42", "longitude": -83.0, "accuracy": 5})",
        R"({"latitude": 90.0001, "longitude": 0, "accuracy": 5})",
        R"({"latitude": 0, "longitude": -180.5, "accuracy": 5})",
        R"({"latitude": 0, "longitude": 0, "accuracy": -1})",
        R"({"latitude": 0, "longitude": 0, "accuracy": 100001})",
        R"({"latitude": null, "longitude": 0, "accuracy": 1})",
    };
    std::size_t refused = 0;
    for (const char* body : bodies) {
        QString error;
        const bool accepted = parsePhonePosition(QByteArray(body), error).has_value();
        refused += (!accepted && !error.isEmpty()) ? 1U : 0U;
    }
    EXPECT_EQ(refused, bodies.size());
}

TEST(PhonePositionTrackerTest, AReportGivesThreeValidSignalsWithTheirUnits) {
    PhonePositionTracker tracker;
    const std::vector<SignalSample> samples =
        tracker.onPosition(PhonePosition{42.0, -83.0, 7.0}, 1000);
    ASSERT_EQ(samples.size(), 3U);
    EXPECT_EQ(samples.at(0).signalId, SignalId::Latitude);
    EXPECT_EQ(samples.at(0).unit, Unit::Degrees);
    EXPECT_DOUBLE_EQ(samples.at(0).value, 42.0);
    EXPECT_EQ(samples.at(1).signalId, SignalId::Longitude);
    EXPECT_DOUBLE_EQ(samples.at(1).value, -83.0);
    EXPECT_EQ(samples.at(2).signalId, SignalId::PositionAccuracy);
    EXPECT_EQ(samples.at(2).unit, Unit::Metres);
    EXPECT_EQ(samples.at(2).timestampMilliseconds, 1000);
    EXPECT_EQ(samples.at(2).status, SignalStatus::Valid);
    EXPECT_EQ(tracker.reportsAccepted(), 1U);
    EXPECT_TRUE(lexus_head_unit::isPhoneSignal(SignalId::Latitude));
    EXPECT_FALSE(lexus_head_unit::isDerivedSignal(SignalId::Latitude));
    EXPECT_FALSE(lexus_head_unit::isPhoneSignal(SignalId::CoolantWarmUpTime));
}

TEST(PhonePositionTrackerTest, AReportAtTheSameMillisecondOrEarlierIsDropped) {
    PhonePositionTracker tracker;
    EXPECT_EQ(tracker.onPosition(PhonePosition{1.0, 2.0, 3.0}, 1000).size(), 3U);
    EXPECT_TRUE(tracker.onPosition(PhonePosition{1.0, 2.0, 3.0}, 1000).empty());
    EXPECT_TRUE(tracker.onPosition(PhonePosition{1.0, 2.0, 3.0}, 999).empty());
    EXPECT_EQ(tracker.reportsAccepted(), 1U);
}

TEST(PhonePositionTrackerTest, FiveSecondsOfSilenceMakeThePositionStaleOnce) {
    PhonePositionTracker tracker;
    EXPECT_TRUE(tracker.checkStaleness(100000).empty()) << "nothing before the first report";
    tracker.onPosition(PhonePosition{1.0, 2.0, 3.0}, 1000);
    EXPECT_TRUE(tracker.checkStaleness(6000).empty()) << "exactly 5 s is still valid";
    const std::vector<SignalSample> stale = tracker.checkStaleness(6001);
    ASSERT_EQ(stale.size(), 3U);
    EXPECT_EQ(stale.at(0).status, SignalStatus::Stale);
    EXPECT_EQ(stale.at(0).timestampMilliseconds, 1000);
    EXPECT_TRUE(tracker.checkStaleness(7000).empty()) << "stale is published once";
    EXPECT_EQ(tracker.onPosition(PhonePosition{1.0, 2.0, 3.0}, 8000).at(0).status,
              SignalStatus::Valid);
}

HttpRequestParser::Result feedAll(HttpRequestParser& parser, const QByteArray& bytes) {
    HttpRequestParser::Result result = HttpRequestParser::Result::Incomplete;
    for (const char byte : bytes) {
        result = parser.feed(QByteArray(1, byte));
    }
    return result;
}

TEST(HttpRequestParserTest, ARequestArrivingByteByByteIsComplete) {
    const QByteArray body = R"({"latitude":1,"longitude":2,"accuracy":3})";
    const QByteArray request = "POST /position HTTP/1.1\r\nHost: pi\r\nContent-Type: "
                               "application/json\r\ncontent-length: " +
                               QByteArray::number(body.size()) + "\r\n\r\n" + body;
    HttpRequestParser parser;
    EXPECT_EQ(feedAll(parser, request), HttpRequestParser::Result::Complete);
    EXPECT_EQ(parser.request().method, QByteArray("POST"));
    EXPECT_EQ(parser.request().path, QByteArray("/position"));
    EXPECT_EQ(parser.request().body, body);
    HttpRequestParser getParser;
    EXPECT_EQ(getParser.feed("GET / HTTP/1.1\r\nHost: pi\r\n\r\n"),
              HttpRequestParser::Result::Complete);
    EXPECT_TRUE(getParser.request().body.isEmpty());
}

TEST(HttpRequestParserTest, MalformedAndOversizedRequestsAreRefused) {
    const std::vector<QByteArray> invalid = {
        "GET\r\n\r\n",
        "GET / FTP/1.0\r\n\r\n",
        "GET nopath HTTP/1.1\r\n\r\n",
        "POST /position HTTP/1.1\r\nContent-Length: twelve\r\n\r\n",
        "POST /position HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
        "POST /position HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "GET / HTTP/1.1\r\nno colon here\r\n\r\n",
    };
    std::size_t refused = 0;
    for (const QByteArray& request : invalid) {
        HttpRequestParser parser;
        refused += parser.feed(request) == HttpRequestParser::Result::Invalid ? 1U : 0U;
    }
    EXPECT_EQ(refused, invalid.size());
    HttpRequestParser bigBody;
    EXPECT_EQ(bigBody.feed("POST /position HTTP/1.1\r\nContent-Length: 4097\r\n\r\n"),
              HttpRequestParser::Result::TooLarge);
    HttpRequestParser bigHead;
    EXPECT_EQ(bigHead.feed("GET / HTTP/1.1\r\nX: " + QByteArray(9000, 'a')),
              HttpRequestParser::Result::TooLarge);
    EXPECT_EQ(bigHead.feed("\r\n\r\n"), HttpRequestParser::Result::TooLarge)
        << "the result does not change";
}

HttpRequest requestOf(const char* method, const char* path, const QByteArray& body = {}) {
    return HttpRequest{QByteArray(method), QByteArray(path), body};
}

TEST(PhoneLinkServerTest, TheRoutesAnswerAsDocumented) {
    const QByteArray page = "<html>page</html>";
    const QByteArray authority = "-----BEGIN CERTIFICATE-----";
    EXPECT_EQ(PhoneLinkServer::respond(requestOf("GET", "/"), page, authority).statusCode, 200);
    EXPECT_TRUE(
        PhoneLinkServer::respond(requestOf("GET", "/?x=1"), page, authority).bytes.endsWith(page));
    const auto certificate = PhoneLinkServer::respond(requestOf("GET", "/ca.crt"), page, authority);
    EXPECT_TRUE(certificate.bytes.contains("Content-Type: application/x-x509-ca-cert"));
    EXPECT_TRUE(certificate.bytes.endsWith(authority));
    const auto accepted = PhoneLinkServer::respond(
        requestOf("POST", "/position", R"({"latitude":1,"longitude":2,"accuracy":3})"),
        page,
        authority);
    EXPECT_EQ(accepted.statusCode, 204);
    ASSERT_TRUE(accepted.position.has_value());
    EXPECT_DOUBLE_EQ(accepted.position.value_or(PhonePosition{}).longitudeDegrees, 2.0);
    const auto refused =
        PhoneLinkServer::respond(requestOf("POST", "/position", "{}"), page, authority);
    EXPECT_EQ(refused.statusCode, 400);
    EXPECT_FALSE(refused.position.has_value());
    EXPECT_EQ(PhoneLinkServer::respond(requestOf("GET", "/position"), page, authority).statusCode,
              405);
    EXPECT_EQ(PhoneLinkServer::respond(requestOf("POST", "/"), page, authority).statusCode, 405);
    EXPECT_EQ(PhoneLinkServer::respond(requestOf("GET", "/etc/passwd"), page, authority).statusCode,
              404);
}

TEST(PhoneLinkServerTest, EveryResponseClosesTheConnectionAndIsNeverCached) {
    const QByteArray bytes = PhoneLinkServer::response(204, {}, {}).bytes;
    EXPECT_TRUE(bytes.startsWith("HTTP/1.1 204 No Content\r\n"));
    EXPECT_TRUE(bytes.contains("Connection: close\r\n"));
    EXPECT_TRUE(bytes.contains("Cache-Control: no-store\r\n"));
    EXPECT_TRUE(bytes.contains("Content-Length: 0\r\n"));
}

TEST(PhoneLinkServerTest, ThePageWatchesThePositionAndPostsIt) {
    const QByteArray page = PhoneLinkServer::page();
    EXPECT_TRUE(page.contains("navigator.geolocation.watchPosition"));
    EXPECT_TRUE(page.contains("fetch(\"/position\""));
    EXPECT_TRUE(page.contains("isSecureContext"));
    EXPECT_FALSE(page.contains("http://")) << "no insecure resource on the page (mixed content)";
    EXPECT_FALSE(page.contains("src=\"http")) << "no external script";
}

} // namespace
// NOLINTEND(misc-include-cleaner)
