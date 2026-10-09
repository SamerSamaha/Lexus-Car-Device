#include "lexus_head_unit/hub/app_process_manager.h"

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/process_launcher.h"
#include "lexus_head_unit/service/clock.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lexus_head_unit {

namespace {

constexpr std::int64_t shutdownPollMilliseconds = 10;
constexpr std::int64_t afterKillWaitMilliseconds = 1000;

AppExitCause causeOf(const ProcessExit& exit) {
    switch (exit.kind) {
    case ProcessExitKind::Exited:
        return exit.code == 0 ? AppExitCause::CleanExit : AppExitCause::FailedExit;
    case ProcessExitKind::Signalled:
        return AppExitCause::Signalled;
    case ProcessExitKind::Lost:
        return AppExitCause::Lost;
    }
    return AppExitCause::Lost;
}

bool isFailure(AppExitCause cause) {
    return cause == AppExitCause::FailedExit || cause == AppExitCause::Signalled;
}

} // namespace

AppProcessManager::AppProcessManager(ProcessLauncher& launcher, const Clock& clock)
    : AppProcessManager(launcher, clock, AppManagerSettings{}) {}

AppProcessManager::AppProcessManager(ProcessLauncher& launcher,
                                     const Clock& clock,
                                     const AppManagerSettings& settings)
    : m_launcher(&launcher), m_clock(&clock), m_settings(settings) {}

LaunchResult AppProcessManager::launch(const AppEntry& entry) {
    if (m_state != AppRunState::Idle) {
        return LaunchResult::AlreadyRunning;
    }
    if (entry.arguments.empty()) {
        return LaunchResult::EmptyCommand;
    }
    m_restartTimesMilliseconds.clear();
    ++m_counters.launches;
    return startProcess(entry);
}

LaunchResult AppProcessManager::startProcess(const AppEntry& entry) {
    const std::optional<ProcessId> processId = m_launcher->start(entry.arguments);
    if (!processId.has_value()) {
        ++m_counters.startFailures;
        m_state = AppRunState::Idle;
        m_entry.reset();
        AppExitReport failure;
        failure.appId = entry.id;
        failure.cause = AppExitCause::StartFailed;
        report(failure);
        return LaunchResult::StartFailed;
    }
    m_entry = entry;
    m_processId = *processId;
    m_startedAtMilliseconds = m_clock->nowMilliseconds();
    m_killSent = false;
    m_state = AppRunState::Running;
    return LaunchResult::Started;
}

bool AppProcessManager::requestStop() {
    if (m_state != AppRunState::Running) {
        return false;
    }
    m_launcher->terminateGroup(m_processId);
    m_state = AppRunState::Stopping;
    m_killAtMilliseconds = m_clock->nowMilliseconds() + m_settings.stopGraceMilliseconds;
    return true;
}

void AppProcessManager::poll() {
    if (m_state == AppRunState::Idle) {
        return;
    }
    const std::optional<ProcessExit> exit = m_launcher->pollExit(m_processId);
    if (exit.has_value()) {
        handleExit(*exit);
        return;
    }
    if (m_state == AppRunState::Stopping && !m_killSent &&
        m_clock->nowMilliseconds() >= m_killAtMilliseconds) {
        m_launcher->killGroup(m_processId);
        m_killSent = true;
        ++m_counters.kills;
    }
}

void AppProcessManager::handleExit(const ProcessExit& exit) {
    ++m_counters.exits;
    if (!m_entry.has_value()) {
        m_state = AppRunState::Idle;
        return;
    }
    const AppEntry entry = *m_entry;
    AppExitReport exitReport;
    exitReport.appId = entry.id;
    exitReport.cause =
        m_state == AppRunState::Stopping ? AppExitCause::StoppedByHub : causeOf(exit);
    exitReport.code = exit.kind == ProcessExitKind::Lost ? 0 : exit.code;
    exitReport.runMilliseconds = m_clock->nowMilliseconds() - m_startedAtMilliseconds;
    m_state = AppRunState::Idle;
    m_entry.reset();
    exitReport.restarting = isFailure(exitReport.cause) &&
                            entry.restart == RestartPolicy::OnFailure && restartAllowed();
    report(exitReport);
    if (exitReport.restarting) {
        ++m_counters.restarts;
        m_restartTimesMilliseconds.push_back(m_clock->nowMilliseconds());
        startProcess(entry);
    }
}

bool AppProcessManager::restartAllowed() {
    const std::int64_t windowStart =
        m_clock->nowMilliseconds() - m_settings.restartWindowMilliseconds;
    while (!m_restartTimesMilliseconds.empty() &&
           m_restartTimesMilliseconds.front() <= windowStart) {
        m_restartTimesMilliseconds.pop_front();
    }
    return m_restartTimesMilliseconds.size() < m_settings.maximumRestartsInWindow;
}

void AppProcessManager::report(const AppExitReport& exitReport) {
    m_lastExit = exitReport;
    if (m_listener) {
        m_listener(exitReport);
    }
}

void AppProcessManager::shutdown(std::int64_t waitMilliseconds) {
    if (m_state == AppRunState::Idle) {
        return;
    }
    if (m_state == AppRunState::Running) {
        requestStop();
    }
    const auto waitUntil = [this](std::int64_t milliseconds) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (m_state != AppRunState::Idle && std::chrono::steady_clock::now() < deadline) {
            poll();
            if (m_state != AppRunState::Idle) {
                std::this_thread::sleep_for(std::chrono::milliseconds(shutdownPollMilliseconds));
            }
        }
    };
    waitUntil(waitMilliseconds);
    if (m_state != AppRunState::Idle && !m_killSent) {
        m_launcher->killGroup(m_processId);
        m_killSent = true;
        ++m_counters.kills;
    }
    waitUntil(afterKillWaitMilliseconds);
}

AppRunState AppProcessManager::runState() const {
    return m_state;
}

std::string AppProcessManager::foregroundAppId() const {
    return m_entry.has_value() ? m_entry->id : std::string();
}

std::optional<ProcessId> AppProcessManager::foregroundProcessId() const {
    if (m_state == AppRunState::Idle) {
        return std::nullopt;
    }
    return m_processId;
}

AppManagerCounters AppProcessManager::counters() const {
    return m_counters;
}

std::optional<AppExitReport> AppProcessManager::lastExit() const {
    return m_lastExit;
}

void AppProcessManager::setExitListener(ExitListener listener) {
    m_listener = std::move(listener);
}

std::string_view toString(AppRunState state) {
    switch (state) {
    case AppRunState::Idle:
        return "idle";
    case AppRunState::Running:
        return "running";
    case AppRunState::Stopping:
        return "stopping";
    }
    return "unknown";
}

std::string_view toString(LaunchResult result) {
    switch (result) {
    case LaunchResult::Started:
        return "started";
    case LaunchResult::AlreadyRunning:
        return "already-running";
    case LaunchResult::StartFailed:
        return "start-failed";
    case LaunchResult::EmptyCommand:
        return "empty-command";
    }
    return "unknown";
}

std::string_view toString(AppExitCause cause) {
    switch (cause) {
    case AppExitCause::CleanExit:
        return "clean-exit";
    case AppExitCause::FailedExit:
        return "failed-exit";
    case AppExitCause::Signalled:
        return "signalled";
    case AppExitCause::StoppedByHub:
        return "stopped-by-hub";
    case AppExitCause::StartFailed:
        return "start-failed";
    case AppExitCause::Lost:
        return "lost";
    }
    return "unknown";
}

std::string describeExit(const AppExitReport& exitReport, std::string_view appName) {
    std::string text(appName);
    switch (exitReport.cause) {
    case AppExitCause::CleanExit:
        text += " closed";
        break;
    case AppExitCause::FailedExit:
        text += " failed (exit code " + std::to_string(exitReport.code) + ")";
        break;
    case AppExitCause::Signalled:
        text += " ended by signal " + std::to_string(exitReport.code);
        break;
    case AppExitCause::StoppedByHub:
        text += " stopped";
        break;
    case AppExitCause::StartFailed:
        text += " could not start";
        break;
    case AppExitCause::Lost:
        text += " was lost";
        break;
    }
    if (exitReport.restarting) {
        text += ", restarting";
    }
    return text;
}

} // namespace lexus_head_unit
