// Verifies: REQ-002

// Runs only where a vcan0 interface exists (the Pi: deploy/setup_vcan.sh). The stock WSL2
// kernel and the CI runners have no vcan module (D-003), so there the tests skip with the
// ticket in the reason. The test itself writes frames with its own raw socket: test tooling may
// send on a virtual bus; the head unit's reader and source cannot (REQ-001).

#include "can_frame_encoder.h"
#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/socket_can_dbc_source.h"
#include "lexus_head_unit/hardware/socket_can_frame_reader.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

// glibc and the kernel headers declare these symbols in internal headers.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::CanFrame;
using lexus_head_unit::CanReadStatus;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::DbcDatabase;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SocketCanDbcSource;
using lexus_head_unit::SocketCanFrameReader;

constexpr const char* interfaceName = "vcan0";

bool vcanPresent() {
    return ::if_nametoindex(interfaceName) != 0;
}

// Test-only sender on the virtual bus.
class VcanSender {
public:
    VcanSender() : m_socket(::socket(PF_CAN, SOCK_RAW, CAN_RAW)) {
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = static_cast<int>(::if_nametoindex(interfaceName));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        m_bound = ::bind(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    }
    VcanSender(const VcanSender&) = delete;
    VcanSender& operator=(const VcanSender&) = delete;
    VcanSender(VcanSender&&) = delete;
    VcanSender& operator=(VcanSender&&) = delete;
    ~VcanSender() {
        ::close(m_socket);
    }

    [[nodiscard]] bool sendFrame(const CanFrame& frame) const {
        can_frame raw{};
        raw.can_id = frame.identifier | (frame.extended ? CAN_EFF_FLAG : 0U);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        raw.len = frame.length;
        for (std::size_t index = 0; index < frame.data.size(); ++index) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            raw.data[index] = frame.data.at(index);
        }
        return m_bound && ::write(m_socket, &raw, sizeof(raw)) == sizeof(raw);
    }

private:
    int m_socket = -1;
    bool m_bound = false;
};

class Collector final : public lexus_head_unit::VehicleDataSourceListener {
public:
    void onSample(const SignalSample& sample) override {
        samples.push_back(sample);
    }
    void onConnectionChanged(const lexus_head_unit::ConnectionTransition& /*transition*/) override {
    }
    std::vector<SignalSample> samples;
};

TEST(VcanTest, RealReaderReceivesAFrameWrittenToVcan0) {
    if (!vcanPresent()) {
        GTEST_SKIP() << "LHU-028: vcan0 is not present (Pi only; sudo deploy/setup_vcan.sh)";
    }
    SocketCanFrameReader reader(interfaceName);
    ASSERT_TRUE(reader.open()) << reader.lastError();
    const DbcDatabase database = DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH);
    const VcanSender sender;
    ASSERT_TRUE(sender.sendFrame(lexus_head_unit::testing::encodeMessage(
        *lexus_head_unit::testing::messageCarrying(database, "VehicleSpeed"),
        {{"VehicleSpeed", 42.0}})));
    const auto result = reader.read(1000);
    ASSERT_EQ(result.status, CanReadStatus::Frame) << reader.lastError();
    EXPECT_EQ(result.frame.identifier, 0x200U);
    EXPECT_EQ(result.frame.length, 8U);
}

TEST(VcanTest, SourcePublishesEngineSpeedFromVcan0) {
    if (!vcanPresent()) {
        GTEST_SKIP() << "LHU-028: vcan0 is not present (Pi only; sudo deploy/setup_vcan.sh)";
    }
    const lexus_head_unit::SteadyClock clock;
    SocketCanFrameReader reader(interfaceName);
    const DbcDatabase database = DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH);
    SocketCanDbcSource source(reader, database, clock, lexus_head_unit::CanSourceConfiguration{});
    Collector collector;
    source.start(collector);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected) << source.lastFailure();
    const VcanSender sender;
    ASSERT_TRUE(sender.sendFrame(lexus_head_unit::testing::encodeMessage(
        *lexus_head_unit::testing::messageCarrying(database, "EngineSpeed"),
        {{"EngineSpeed", 1234.5}})));
    for (int run = 0; run < 20 && collector.samples.empty(); ++run) {
        source.runOnce();
    }
    std::optional<double> rpm;
    for (const SignalSample& sample : collector.samples) {
        if (sample.signalId == SignalId::EngineRpm) {
            rpm = sample.value;
        }
    }
    EXPECT_EQ(rpm, std::optional<double>(1234.5));
    source.stop();
}

} // namespace
// NOLINTEND(misc-include-cleaner)
