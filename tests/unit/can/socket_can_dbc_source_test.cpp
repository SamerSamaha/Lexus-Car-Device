// Verifies: REQ-001, REQ-002

#include "can_frame_encoder.h"
#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/fake_can_frame_reader.h"
#include "lexus_head_unit/hardware/socket_can_dbc_source.h"
#include "lexus_head_unit/hardware/socket_can_frame_reader.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::CanFrame;
using lexus_head_unit::CanSourceConfiguration;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::DbcDatabase;
using lexus_head_unit::FakeCanFrameReader;
using lexus_head_unit::KeyValueConfiguration;
using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SocketCanDbcSource;
using lexus_head_unit::Unit;
using lexus_head_unit::testing::encodeMessage;
using lexus_head_unit::testing::messageCarrying;

// REQ-001 at compile time: none of the CAN types may ever gain a member that transmits.
template <typename Type, typename = void>
struct HasWrite : std::false_type {};
template <typename Type>
struct HasWrite<Type, std::void_t<decltype(&Type::write)>> : std::true_type {};
template <typename Type, typename = void>
struct HasSend : std::false_type {};
template <typename Type>
struct HasSend<Type, std::void_t<decltype(&Type::send)>> : std::true_type {};
template <typename Type, typename = void>
struct HasTransmit : std::false_type {};
template <typename Type>
struct HasTransmit<Type, std::void_t<decltype(&Type::transmit)>> : std::true_type {};

template <typename Type>
constexpr bool cannotTransmit =
    std::negation_v<std::disjunction<HasWrite<Type>, HasSend<Type>, HasTransmit<Type>>>;

static_assert(cannotTransmit<lexus_head_unit::CanFrameReader>);
static_assert(cannotTransmit<lexus_head_unit::SocketCanFrameReader>);
static_assert(cannotTransmit<FakeCanFrameReader>);
static_assert(cannotTransmit<SocketCanDbcSource>);

// The detection itself works: a type with such a member is caught.
struct Transmitter {
    void write() {}
};
static_assert(!cannotTransmit<Transmitter>);

class Recorder final : public lexus_head_unit::VehicleDataSourceListener {
public:
    void onSample(const SignalSample& sample) override {
        samples.push_back(sample);
    }
    void onConnectionChanged(const ConnectionTransition& transition) override {
        transitions.push_back(transition);
    }
    std::vector<SignalSample> samples;
    std::vector<ConnectionTransition> transitions;
};

class SocketCanDbcSourceTest : public ::testing::Test {
protected:
    DbcDatabase m_database = DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH);
    ManualClock m_clock{5000};
    FakeCanFrameReader m_reader;
    Recorder m_recorder;
    SocketCanDbcSource m_source{m_reader, m_database, m_clock, CanSourceConfiguration{}};

    CanFrame frameWith(const std::string& signalName, double value) {
        return encodeMessage(*messageCarrying(m_database, signalName), {{signalName, value}});
    }
};

TEST_F(SocketCanDbcSourceTest, ConnectsAndPublishesMappedSignalsWithProjectUnits) {
    m_source.start(m_recorder);
    ASSERT_EQ(m_source.connectionState(), ConnectionState::Connected);
    m_reader.queueFrame(frameWith("VehicleSpeed", 88.5));
    m_reader.queueFrame(encodeMessage(*messageCarrying(m_database, "EngineSpeed"),
                                      {{"EngineSpeed", 2500.0}, {"CoolantTemperature", 90.0}}));
    m_source.runOnce();
    std::map<SignalId, std::pair<double, Unit>> latest;
    bool everyTimestampIsTheReadTime = true;
    for (const SignalSample& sample : m_recorder.samples) {
        latest[sample.signalId] = {sample.value, sample.unit};
        everyTimestampIsTheReadTime =
            everyTimestampIsTheReadTime && sample.timestampMilliseconds == 5000;
    }
    EXPECT_TRUE(everyTimestampIsTheReadTime);
    const std::map<SignalId, std::pair<double, Unit>> expected = {
        {SignalId::VehicleSpeed, {88.5, Unit::KilometresPerHour}},
        {SignalId::EngineRpm, {2500.0, Unit::RevolutionsPerMinute}},
        {SignalId::CoolantTemperature, {90.0, Unit::DegreesCelsius}},
        {SignalId::EngineLoad, {0.0, Unit::Percent}},
        {SignalId::ThrottlePosition, {0.0, Unit::Percent}},
        {SignalId::IntakeAirTemperature, {0.0, Unit::DegreesCelsius}},
    };
    EXPECT_EQ(latest, expected);
    // VehicleMotion carries one mapped signal of four; EngineStatus five of six.
    EXPECT_EQ(std::make_pair(m_recorder.samples.size(), m_source.counters().samplesEmitted),
              std::make_pair(std::size_t{6}, std::uint64_t{6}));
}

TEST_F(SocketCanDbcSourceTest, MalformedFramesAreCountedAndGiveNoSample) {
    m_source.start(m_recorder);
    CanFrame shortFrame = frameWith("VehicleSpeed", 10.0);
    shortFrame.length = 7;
    CanFrame unknown = frameWith("VehicleSpeed", 10.0);
    unknown.identifier = 0x555;
    CanFrame invalid = frameWith("VehicleSpeed", 10.0);
    invalid.length = 12;
    for (const CanFrame& frame : {shortFrame, unknown, invalid}) {
        m_reader.queueFrame(frame);
    }
    m_source.runOnce();
    EXPECT_TRUE(m_recorder.samples.empty());
    EXPECT_EQ(m_source.counters().malformedInputs, 3U);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
}

TEST_F(SocketCanDbcSourceTest, SilenceForTwoSecondsIsLinkLossThenBackoffReconnects) {
    m_source.start(m_recorder);
    m_clock.advanceMilliseconds(1999);
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
    m_clock.advanceMilliseconds(1);
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Error);
    EXPECT_EQ(m_recorder.transitions.back().trigger, ConnectionTrigger::LinkLost);
    EXPECT_EQ(m_source.lastFailure(), "no frame for 2000 ms");
    EXPECT_EQ(m_source.idleHintMilliseconds(), 1000);
    m_clock.advanceMilliseconds(1000);
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
}

TEST_F(SocketCanDbcSourceTest, ReaderErrorIsLinkLossAndFailedOpensFollowTheBackoffSchedule) {
    m_source.start(m_recorder);
    m_reader.breakLink();
    m_source.runOnce();
    ASSERT_EQ(m_source.connectionState(), ConnectionState::Error);
    std::vector<std::int64_t> delays;
    for (int attempt = 0; attempt < 5; ++attempt) {
        delays.push_back(m_source.idleHintMilliseconds());
        m_clock.advanceMilliseconds(m_source.idleHintMilliseconds());
        m_source.runOnce();
        EXPECT_EQ(m_source.connectionState(), ConnectionState::Error);
    }
    EXPECT_EQ(delays, (std::vector<std::int64_t>{1000, 2000, 4000, 8000, 10000}));
    m_reader.restoreLink();
    m_clock.advanceMilliseconds(m_source.idleHintMilliseconds());
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(m_source.connectionAttempts(), 7U);
}

TEST(SocketCanDbcSourceConfigurationTest, UnitMismatchOrDbcErrorFailsTheHandshake) {
    const ManualClock clock{0};
    FakeCanFrameReader reader;
    Recorder recorder;
    SocketCanDbcSource wrongUnit(
        reader,
        DbcDatabase::parse("BO_ 1 A: 8 X\n SG_ VehicleSpeed : 0|8@1+ (1,0) [0|1] \"mph\" Y\n"),
        clock,
        CanSourceConfiguration{});
    wrongUnit.start(recorder);
    EXPECT_EQ(wrongUnit.connectionState(), ConnectionState::Error);
    EXPECT_EQ(wrongUnit.lastFailure(), "DBC signal VehicleSpeed has unit 'mph', expected 'km/h'");
    EXPECT_EQ(reader.openCount(), 0);

    SocketCanDbcSource brokenDbc(
        reader, DbcDatabase::loadFromFile("/nonexistent.dbc"), clock, CanSourceConfiguration{});
    brokenDbc.start(recorder);
    EXPECT_EQ(brokenDbc.connectionState(), ConnectionState::Error);
    EXPECT_EQ(brokenDbc.lastFailure(), "DBC error: cannot open /nonexistent.dbc");
}

TEST(SocketCanDbcSourceConfigurationTest, KeysAndNameMapping) {
    KeyValueConfiguration file;
    file.loadFromText("[can]\ninterface = can1\ndbc = /etc/car.dbc\nlink_loss_timeout_ms = 3000\n");
    const CanSourceConfiguration configuration = CanSourceConfiguration::fromConfiguration(file);
    EXPECT_EQ(configuration.interfaceName, "can1");
    EXPECT_EQ(configuration.dbcPath, "/etc/car.dbc");
    EXPECT_EQ(configuration.linkLossTimeoutMilliseconds, 3000);
    EXPECT_EQ(configuration.readTimeoutMilliseconds, 50);
    EXPECT_EQ(lexus_head_unit::signalIdForDbcName("EngineSpeed"),
              std::optional<SignalId>(SignalId::EngineRpm));
    EXPECT_EQ(lexus_head_unit::signalIdForDbcName("Odometer"), std::nullopt);
}

TEST(SocketCanFrameReaderTest, MissingInterfaceFailsToOpenWithAReason) {
    lexus_head_unit::SocketCanFrameReader reader("lexusnone0");
    EXPECT_FALSE(reader.open());
    EXPECT_FALSE(reader.isOpen());
    EXPECT_NE(reader.lastError().find("lexusnone0"), std::string::npos) << reader.lastError();
    EXPECT_EQ(reader.read(0).status, lexus_head_unit::CanReadStatus::Closed);
    lexus_head_unit::SocketCanFrameReader badName("");
    EXPECT_FALSE(badName.open());
}

} // namespace
