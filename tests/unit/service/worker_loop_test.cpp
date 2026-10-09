// Verifies: REQ-002

#include "lexus_head_unit/hardware/fake_source.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/worker_loop.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::FakeSource;
using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::WorkerLoop;

class WorkerLoopTest : public ::testing::Test {
protected:
    ManualClock m_clock{1000};
    SignalStore m_store;
    StalenessMonitor m_monitor{m_store, m_clock};
    SignalStoreFeeder m_feeder{m_store};
    FakeSource m_source{m_clock};
    std::atomic<int> m_cycles{0};

    // Starts the loop, waits up to five seconds for the cycle count, stops; returns the count.
    int runLoopForCycles(WorkerLoop& loop, int target) {
        loop.setPerCycleCallback([this]() {
            m_cycles.fetch_add(1);
        });
        loop.start();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (m_cycles.load() < target && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        loop.stop();
        return m_cycles.load();
    }
};

TEST_F(WorkerLoopTest, StartIsIdempotentAndStopEndsTheThread) {
    WorkerLoop loop(m_source, m_feeder, m_monitor, 5);
    EXPECT_FALSE(loop.isRunning());
    loop.start();
    EXPECT_TRUE(loop.isRunning());
    loop.start(); // a second start changes nothing
    EXPECT_TRUE(loop.isRunning());
    loop.stop();
    EXPECT_FALSE(loop.isRunning());
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Disconnected);
}

TEST_F(WorkerLoopTest, RunsCyclesAndDeliversScriptedSamplesToTheStore) {
    m_source.scriptSample(SignalId::VehicleSpeed, 42.0);
    WorkerLoop loop(m_source, m_feeder, m_monitor, 5);
    const int reached = runLoopForCycles(loop, 3);
    EXPECT_GE(reached, 3);
    EXPECT_GE(loop.cycleCount(), 3U);
    EXPECT_EQ(m_feeder.acceptedSampleCount(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 42.0);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Disconnected);
}

TEST_F(WorkerLoopTest, DestructorStopsARunningLoop) {
    {
        WorkerLoop loop(m_source, m_feeder, m_monitor, 5);
        loop.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Disconnected);
}

} // namespace
