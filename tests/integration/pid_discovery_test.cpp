// Verifies: REQ-004, REQ-001, REQ-022

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

// One full pass over the poll list per cycle.
void pollCycles(Elm327ObdSource& source, std::size_t cycles) {
    const std::size_t pollListSize = source.pollList().size();
    for (std::size_t cycle = 0; cycle < cycles; ++cycle) {
        for (std::size_t poll = 0; poll < pollListSize; ++poll) {
            source.runOnce();
        }
    }
}

TEST(PidDiscoveryTest, UnsupportedPidIsNeverRequestedOverOneHundredPollingCycles) {
    EmulatorProcess emulator({"--profile", "no-fuel-level"});
    ASSERT_TRUE(emulator.start()) << emulator.lastError();
    FileDescriptorByteTransport transport(emulator.linkPath());
    const SteadyClock clock;
    Elm327SourceConfiguration configuration;
    configuration.devicePath = emulator.linkPath();
    Elm327ObdSource source(transport, clock, configuration);
    SignalStore store;
    SignalStoreFeeder feeder(store);

    source.start(feeder);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);
    EXPECT_FALSE(source.supportedPids().contains(0x2F));
    EXPECT_TRUE(source.supportedPids().contains(0x0D));
    ASSERT_EQ(source.pollList().size(), 8U);

    pollCycles(source, 100);
    source.stop();

    EXPECT_EQ(emulator.requestCount("012F"), 0);
    EXPECT_GE(emulator.requestCount("010D"), 100);
    EXPECT_GE(emulator.requestCount("0142"), 100);
    EXPECT_GE(emulator.requestCount("0110"), 100);
    EXPECT_EQ(emulator.forbiddenRequestCount(), 0);
    EXPECT_EQ(store.latest(SignalId::FuelLevel).status, SignalStatus::NeverReceived);
    EXPECT_EQ(store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_EQ(store.latest(SignalId::MassAirFlow).status, SignalStatus::Valid);
    // 800 requests answered (8 PIDs, 100 cycles); a sample is rejected only when it lands in the
    // same millisecond as the previous one of its signal, which the emulator can do and the
    // adapter cannot.
    EXPECT_EQ(source.counters().samplesEmitted, 800U);
    EXPECT_EQ(feeder.acceptedSampleCount() + feeder.rejectedSampleCount(), 800U);
    EXPECT_EQ(feeder.rejectedSampleCount(), store.rejectedOutOfOrderCount());
    EXPECT_GE(feeder.acceptedSampleCount(), 100U);
    EXPECT_EQ(source.counters().malformedInputs, 0U);
}

} // namespace
