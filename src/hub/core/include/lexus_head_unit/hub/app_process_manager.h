#pragma once

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/process_launcher.h"
#include "lexus_head_unit/service/clock.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace lexus_head_unit {

enum class AppRunState {
    Idle,
    Running,
    Stopping,
};

enum class LaunchResult {
    Started,
    AlreadyRunning,
    StartFailed,
    EmptyCommand,
};

enum class AppExitCause {
    CleanExit,
    FailedExit,
    Signalled,
    StoppedByHub,
    StartFailed,
    Lost,
};

struct AppExitReport {
    std::string appId;
    AppExitCause cause = AppExitCause::CleanExit;
    // Exit status for FailedExit, signal number for Signalled, 0 otherwise.
    int code = 0;
    std::int64_t runMilliseconds = 0;
    bool restarting = false;
};

struct AppManagerSettings {
    std::int64_t stopGraceMilliseconds = 3000;
    std::int64_t restartWindowMilliseconds = 60000;
    std::size_t maximumRestartsInWindow = 3;
};

struct AppManagerCounters {
    std::uint64_t launches = 0;
    std::uint64_t exits = 0;
    std::uint64_t restarts = 0;
    std::uint64_t startFailures = 0;
    std::uint64_t kills = 0;
};

// Runs at most one foreground app (DN-021). Not thread-safe: the hub calls every method from
// its UI thread, poll() from a timer.
class AppProcessManager {
public:
    using ExitListener = std::function<void(const AppExitReport&)>;

    AppProcessManager(ProcessLauncher& launcher, const Clock& clock);
    AppProcessManager(ProcessLauncher& launcher,
                      const Clock& clock,
                      const AppManagerSettings& settings);

    LaunchResult launch(const AppEntry& entry);
    // Asks the foreground app to end: SIGTERM to its group now, SIGKILL after the grace period.
    bool requestStop();
    // Reaps an ended app, escalates a stop that is overdue, applies the restart policy.
    void poll();
    // Stops the foreground app and waits for it, polling every 10 ms, for at most
    // waitMilliseconds of real time; kills it if it is still there. For the hub's own exit.
    void shutdown(std::int64_t waitMilliseconds);

    [[nodiscard]] AppRunState runState() const;
    [[nodiscard]] std::string foregroundAppId() const;
    [[nodiscard]] std::optional<ProcessId> foregroundProcessId() const;
    [[nodiscard]] AppManagerCounters counters() const;
    [[nodiscard]] std::optional<AppExitReport> lastExit() const;
    void setExitListener(ExitListener listener);

private:
    LaunchResult startProcess(const AppEntry& entry);
    void handleExit(const ProcessExit& exit);
    bool restartAllowed();
    void report(const AppExitReport& exitReport);

    ProcessLauncher* m_launcher;
    const Clock* m_clock;
    AppManagerSettings m_settings;
    AppRunState m_state = AppRunState::Idle;
    std::optional<AppEntry> m_entry;
    ProcessId m_processId = 0;
    std::int64_t m_startedAtMilliseconds = 0;
    std::int64_t m_killAtMilliseconds = 0;
    bool m_killSent = false;
    std::deque<std::int64_t> m_restartTimesMilliseconds;
    AppManagerCounters m_counters;
    std::optional<AppExitReport> m_lastExit;
    ExitListener m_listener;
};

std::string_view toString(AppRunState state);
std::string_view toString(LaunchResult result);
std::string_view toString(AppExitCause cause);
// "Vehicle data exited (code 3)" style text for the status strip.
std::string describeExit(const AppExitReport& exitReport, std::string_view appName);

} // namespace lexus_head_unit
