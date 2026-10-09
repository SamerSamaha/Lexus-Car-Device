#include "lexus_head_unit/service/worker_loop.h"

#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>

namespace lexus_head_unit {

namespace {

constexpr std::int64_t maximumSleepMilliseconds = 50;

} // namespace

WorkerLoop::WorkerLoop(VehicleDataSource& source,
                       VehicleDataSourceListener& listener,
                       StalenessMonitor& monitor,
                       std::int64_t minimumCycleMilliseconds)
    : m_source(&source),
      m_listener(&listener),
      m_monitor(&monitor),
      m_minimumCycleMilliseconds(minimumCycleMilliseconds) {}

WorkerLoop::~WorkerLoop() {
    stop();
}

void WorkerLoop::setPerCycleCallback(PerCycleCallback callback) {
    m_perCycle = std::move(callback);
}

void WorkerLoop::start() {
    if (m_running.load()) {
        return;
    }
    m_stopRequested.store(false);
    m_running.store(true);
    m_thread = std::thread([this]() {
        run();
    });
}

void WorkerLoop::stop() {
    m_stopRequested.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_running.store(false);
}

bool WorkerLoop::isRunning() const {
    return m_running.load();
}

std::uint64_t WorkerLoop::cycleCount() const {
    return m_cycleCount.load();
}

void WorkerLoop::run() {
    m_source->start(*m_listener);
    while (!m_stopRequested.load()) {
        m_source->runOnce();
        m_monitor->check();
        if (m_perCycle) {
            m_perCycle();
        }
        m_cycleCount.fetch_add(1);
        const std::int64_t hint =
            std::max(m_source->idleHintMilliseconds(), m_minimumCycleMilliseconds);
        const std::int64_t sleepFor = std::clamp<std::int64_t>(hint, 0, maximumSleepMilliseconds);
        if (sleepFor > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepFor));
        }
    }
    m_source->stop();
}

} // namespace lexus_head_unit
