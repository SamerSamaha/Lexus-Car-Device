#pragma once

#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

namespace lexus_head_unit {

// Owns the worker thread: starts the source, then repeats runOnce, the staleness check, an
// optional per-cycle callback and a sleep for the source's idle hint (capped at 50 ms).
class WorkerLoop {
public:
    using PerCycleCallback = std::function<void()>;

    WorkerLoop(VehicleDataSource& source,
               VehicleDataSourceListener& listener,
               StalenessMonitor& monitor,
               std::int64_t minimumCycleMilliseconds = 0);
    ~WorkerLoop();

    WorkerLoop(const WorkerLoop&) = delete;
    WorkerLoop& operator=(const WorkerLoop&) = delete;
    WorkerLoop(WorkerLoop&&) = delete;
    WorkerLoop& operator=(WorkerLoop&&) = delete;

    void setPerCycleCallback(PerCycleCallback callback);
    void start();
    void stop();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] std::uint64_t cycleCount() const;

private:
    void run();

    VehicleDataSource* m_source;
    VehicleDataSourceListener* m_listener;
    StalenessMonitor* m_monitor;
    std::int64_t m_minimumCycleMilliseconds;
    PerCycleCallback m_perCycle;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_running{false};
    std::atomic<std::uint64_t> m_cycleCount{0};
};

} // namespace lexus_head_unit
