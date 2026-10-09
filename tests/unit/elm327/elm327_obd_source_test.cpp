// Verifies: REQ-004, REQ-008, REQ-010, REQ-002

#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::ManualClock;
using lexus_head_unit::ObdPid;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::Unit;
using lexus_head_unit::VehicleDataSourceListener;

bool commandWasWritten(const FakeByteTransport& transport, const std::string& command) {
    const auto& written = transport.writtenCommands();
    return std::any_of(written.begin(), written.end(), [&command](const std::string& candidate) {
        return candidate == command;
    });
}

// Every Mode 01 request now gets no reply at all.
void silenceEveryPid(FakeByteTransport& transport) {
    for (const char* request : {"010D", "010C", "0105", "0104", "0111", "010F", "0142", "012F"}) {
        transport.replaceReply(request, "");
    }
}

struct RetryObservation {
    std::vector<std::int64_t> intervalsMilliseconds;
    std::vector<std::uint64_t> attemptsBeforeDue;
    std::vector<std::uint64_t> attemptsWhenDue;
};

// Drives the clock to each scheduled attempt, one millisecond early and then on time.
RetryObservation observeRetries(Elm327ObdSource& source, ManualClock& clock, int retries) {
    RetryObservation observation;
    std::int64_t lastAttemptAt = clock.nowMilliseconds();
    for (int retry = 0; retry < retries; ++retry) {
        const std::int64_t attemptAt = source.nextAttemptAtMilliseconds();
        clock.setMilliseconds(attemptAt - 1);
        source.runOnce();
        observation.attemptsBeforeDue.push_back(source.connectionAttempts());
        clock.setMilliseconds(attemptAt);
        source.runOnce();
        observation.attemptsWhenDue.push_back(source.connectionAttempts());
        observation.intervalsMilliseconds.push_back(attemptAt - lastAttemptAt);
        lastAttemptAt = attemptAt;
    }
    return observation;
}

class RecordingListener final : public VehicleDataSourceListener {
public:
    std::vector<SignalSample> samples;
    std::vector<ConnectionTransition> transitions;

    void onSample(const SignalSample& sample) override {
        samples.push_back(sample);
    }

    void onConnectionChanged(const ConnectionTransition& transition) override {
        transitions.push_back(transition);
    }
};

// A fake adapter on a running engine: setup commands, three bitmaps and the 8 PIDs.
void scriptHealthyAdapter(FakeByteTransport& transport) {
    transport.scriptReply("ATZ", "ATZ\rELM327 v1.5\r\r>");
    for (const char* setup : {"ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"}) {
        transport.scriptReply(setup, "OK\r\r>");
    }
    transport.scriptReply("ATI", "ELM327 v1.5\r\r>");
    transport.scriptReply("ATRV", "14.1V\r\r>");
    transport.scriptReply("ATDPN", "A6\r\r>");
    transport.scriptReply("0100", "4100BE3FA813\r\r>");
    transport.scriptReply("0120", "41208007A001\r\r>");
    transport.scriptReply("0140", "4140FED00400\r\r>");
    transport.scriptReply("010D", "410D3C\r\r>");
    transport.scriptReply("010C", "410C1AF8\r\r>");
    transport.scriptReply("0105", "41057B\r\r>");
    transport.scriptReply("0104", "410433\r\r>");
    transport.scriptReply("0111", "411119\r\r>");
    transport.scriptReply("010F", "410F41\r\r>");
    transport.scriptReply("0142", "41423714\r\r>");
    transport.scriptReply("012F", "412FA0\r\r>");
}

class Elm327ObdSourceTest : public ::testing::Test {
protected:
    FakeByteTransport m_transport;
    ManualClock m_clock{100000};
    Elm327SourceConfiguration m_configuration;
    RecordingListener m_listener;

    Elm327SourceConfiguration configuration() {
        m_configuration.devicePath = "fake";
        return m_configuration;
    }
};

TEST_F(Elm327ObdSourceTest, StartRunsTheSetupSequenceDiscoversPidsAndConnects) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    EXPECT_EQ(source.name(), "elm327");

    source.start(m_listener);

    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);
    const std::vector<std::string> expectedCommands = {"ATZ",
                                                       "ATE0",
                                                       "ATL0",
                                                       "ATS0",
                                                       "ATH0",
                                                       "ATSP0",
                                                       "ATI",
                                                       "ATRV",
                                                       "ATDPN",
                                                       "0100",
                                                       "0120",
                                                       "0140"};
    EXPECT_EQ(m_transport.writtenCommands(), expectedCommands);
    EXPECT_EQ(source.pollList().size(), 8U);
    EXPECT_EQ(source.pollList().front(), ObdPid::VehicleSpeed);
    EXPECT_EQ(source.adapterIdentity(), "ELM327V1.5");
    EXPECT_EQ(source.adapterVoltageText(), "14.1V");
    EXPECT_EQ(source.protocolNumberText(), "A6");
    EXPECT_EQ(source.connectionAttempts(), 1U);
    ASSERT_EQ(m_listener.transitions.size(), 2U);
    EXPECT_EQ(m_listener.transitions.back().trigger, ConnectionTrigger::HandshakeSucceeded);
}

TEST_F(Elm327ObdSourceTest, EachRunOncePollsTheNextPidAndEmitsADecodedSampleWithTheClockTime) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    const std::size_t commandsAfterStart = m_transport.writtenCommands().size();

    m_clock.setMilliseconds(100500);
    source.runOnce();
    m_clock.setMilliseconds(100600);
    source.runOnce();

    ASSERT_EQ(m_listener.samples.size(), 2U);
    EXPECT_EQ(m_listener.samples.at(0).signalId, SignalId::VehicleSpeed);
    EXPECT_DOUBLE_EQ(m_listener.samples.at(0).value, 60.0);
    EXPECT_EQ(m_listener.samples.at(0).unit, Unit::KilometresPerHour);
    EXPECT_EQ(m_listener.samples.at(0).timestampMilliseconds, 100500);
    EXPECT_EQ(m_listener.samples.at(1).signalId, SignalId::EngineRpm);
    EXPECT_DOUBLE_EQ(m_listener.samples.at(1).value, 1726.0);
    EXPECT_EQ(m_listener.samples.at(1).timestampMilliseconds, 100600);
    EXPECT_EQ(m_transport.writtenCommands().at(commandsAfterStart), "010D");
    EXPECT_EQ(m_transport.writtenCommands().at(commandsAfterStart + 1), "010C");
    EXPECT_EQ(source.counters().samplesEmitted, 2U);
    EXPECT_EQ(source.counters().requestsSent, 14U);
}

TEST_F(Elm327ObdSourceTest, PollListWrapsAroundAllEightPidsInOrder) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    for (int cycle = 0; cycle < 9; ++cycle) {
        m_clock.advanceMilliseconds(10);
        source.runOnce();
    }
    ASSERT_EQ(m_listener.samples.size(), 9U);
    EXPECT_EQ(m_listener.samples.at(7).signalId, SignalId::FuelLevel);
    EXPECT_EQ(m_listener.samples.at(8).signalId, SignalId::VehicleSpeed);
}

TEST_F(Elm327ObdSourceTest, OnlySupportedPidsArePolled) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("0100", "4100BE3FA813\r\r>");
    m_transport.replaceReply("0120", "41208000A000\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    EXPECT_EQ(source.pollList().size(), 6U);
    EXPECT_FALSE(source.supportedPids().contains(0x2F));
    EXPECT_FALSE(source.supportedPids().contains(0x42));
    for (int cycle = 0; cycle < 12; ++cycle) {
        m_clock.advanceMilliseconds(10);
        source.runOnce();
    }
    EXPECT_FALSE(commandWasWritten(m_transport, "012F"));
    EXPECT_FALSE(commandWasWritten(m_transport, "0142"));
    EXPECT_FALSE(commandWasWritten(m_transport, "0140"));
}

TEST_F(Elm327ObdSourceTest, NamedErrorRepliesAreCountedWithoutASampleAndStayConnected) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("010D", "NO DATA\r\r>");
    m_transport.replaceReply("010C", "UNABLE TO CONNECT\r\r>");
    m_transport.replaceReply("0105", "41 05 ZZ\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    for (int cycle = 0; cycle < 3; ++cycle) {
        m_clock.advanceMilliseconds(10);
        source.runOnce();
    }
    EXPECT_TRUE(m_listener.samples.empty());
    EXPECT_EQ(source.counters().malformedInputs, 3U);
    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);
}

TEST_F(Elm327ObdSourceTest, TruncatedDataReplyIsCountedAsMalformed) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("010D", "410D\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    source.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());
    EXPECT_EQ(source.counters().malformedInputs, 1U);
}

TEST_F(Elm327ObdSourceTest, LinkIsLostAfterTwoSilentRequestsAtTwoSeconds) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    m_clock.setMilliseconds(100500);
    source.runOnce(); // one good reply at 100500
    ASSERT_EQ(m_listener.samples.size(), 1U);
    silenceEveryPid(m_transport);

    m_clock.setMilliseconds(101000);
    source.runOnce(); // silent, 500 ms since the last reply
    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);
    m_clock.setMilliseconds(102000);
    source.runOnce(); // silent, 1500 ms since the last reply
    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);
    m_clock.setMilliseconds(102500);
    source.runOnce(); // silent, 2000 ms since the last reply: lost
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_FALSE(m_transport.isOpen());
    EXPECT_EQ(source.nextAttemptAtMilliseconds(), 103500);
    EXPECT_EQ(source.idleHintMilliseconds(), 1000);
    EXPECT_EQ(m_listener.transitions.back().trigger, ConnectionTrigger::LinkLost);
}

TEST_F(Elm327ObdSourceTest, EndOfFileLosesTheLinkAtOnce) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    m_transport.close();
    source.runOnce();
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_EQ(m_listener.transitions.back().trigger, ConnectionTrigger::LinkLost);
}

TEST_F(Elm327ObdSourceTest, RetriesFollowTheBackoffScheduleOnTheClockAndResetAfterSuccess) {
    scriptHealthyAdapter(m_transport);
    m_transport.setOpenable(false);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_EQ(source.connectionAttempts(), 1U);

    const RetryObservation observed = observeRetries(source, m_clock, 6);
    EXPECT_EQ(observed.attemptsBeforeDue, (std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(observed.attemptsWhenDue, (std::vector<std::uint64_t>{2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(observed.intervalsMilliseconds,
              (std::vector<std::int64_t>{1000, 2000, 4000, 8000, 10000, 10000}));

    m_transport.setOpenable(true);
    m_clock.setMilliseconds(source.nextAttemptAtMilliseconds());
    source.runOnce();
    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);

    m_transport.close();
    source.runOnce();
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_EQ(source.nextAttemptAtMilliseconds() - m_clock.nowMilliseconds(), 1000);
}

TEST_F(Elm327ObdSourceTest, HandshakeFailsWhenASetupCommandDoesNotAnswerOk) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("ATSP0", "?\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_FALSE(m_transport.isOpen());
    EXPECT_EQ(m_listener.transitions.back().trigger, ConnectionTrigger::HandshakeFailed);
}

TEST_F(Elm327ObdSourceTest, HandshakeFailsWhenTheVehicleGivesNoBitmap) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("0100", "UNABLE TO CONNECT\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    EXPECT_EQ(source.connectionState(), ConnectionState::Error);
    EXPECT_TRUE(source.pollList().empty());
}

TEST_F(Elm327ObdSourceTest, DiscoveryStopsAtTheFirstBitmapWithoutAChainBit) {
    scriptHealthyAdapter(m_transport);
    m_transport.replaceReply("0100", "4100BE3FA812\r\r>");
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    EXPECT_EQ(source.connectionState(), ConnectionState::Connected);
    EXPECT_FALSE(commandWasWritten(m_transport, "0120"));
    EXPECT_EQ(source.pollList().size(), 6U);
}

TEST_F(Elm327ObdSourceTest, StopFromConnectedErrorAndConnectingDisconnectsAndClosesTheTransport) {
    scriptHealthyAdapter(m_transport);
    Elm327ObdSource source(m_transport, m_clock, configuration());
    source.start(m_listener);
    source.stop();
    EXPECT_EQ(source.connectionState(), ConnectionState::Disconnected);
    EXPECT_FALSE(m_transport.isOpen());
    source.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());

    m_transport.setOpenable(false);
    Elm327ObdSource failing(m_transport, m_clock, configuration());
    failing.start(m_listener);
    EXPECT_EQ(failing.connectionState(), ConnectionState::Error);
    failing.stop();
    EXPECT_EQ(failing.connectionState(), ConnectionState::Disconnected);
    EXPECT_EQ(failing.counters().rejectedTransitions, 0U);
    failing.stop();
    EXPECT_EQ(failing.counters().rejectedTransitions, 0U);
}

TEST_F(Elm327ObdSourceTest, IdleHintsFollowTheState) {
    scriptHealthyAdapter(m_transport);
    m_configuration.pollIntervalMilliseconds = 50;
    Elm327ObdSource source(m_transport, m_clock, configuration());
    EXPECT_EQ(source.idleHintMilliseconds(), 0);
    source.start(m_listener);
    EXPECT_EQ(source.idleHintMilliseconds(), 50);

    FakeByteTransport minimal;
    scriptHealthyAdapter(minimal);
    minimal.replaceReply("0100", "410000000000\r\r>");
    Elm327ObdSource nothingToPoll(minimal, m_clock, configuration());
    nothingToPoll.start(m_listener);
    EXPECT_EQ(nothingToPoll.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(nothingToPoll.idleHintMilliseconds(), 1000);
    nothingToPoll.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());
}

TEST_F(Elm327ObdSourceTest, DefaultConfigurationMatchesTheRequirements) {
    const Elm327SourceConfiguration defaults;
    EXPECT_EQ(defaults.replyTimeoutMilliseconds, 1000);
    EXPECT_EQ(defaults.linkLossTimeoutMilliseconds, 2000);
    EXPECT_EQ(defaults.backoffScheduleMilliseconds,
              (std::vector<std::int64_t>{1000, 2000, 4000, 8000}));
    EXPECT_EQ(defaults.backoffCapMilliseconds, 10000);
    EXPECT_EQ(defaults.devicePath, "/dev/rfcomm0");
}

} // namespace
