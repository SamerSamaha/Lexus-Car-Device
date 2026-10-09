#pragma once

#include "lexus_head_unit/hub/process_launcher.h"

#include <csignal>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lexus_head_unit::testing {

// A ProcessLauncher that starts nothing. Tests decide when each "process" ends and how.
class FakeProcessLauncher final : public ProcessLauncher {
public:
    std::optional<ProcessId> start(const std::vector<std::string>& arguments) override {
        m_startedArguments.push_back(arguments);
        if (m_failStarts) {
            return std::nullopt;
        }
        const ProcessId processId = m_nextProcessId++;
        m_lastProcessId = processId;
        return processId;
    }

    std::optional<ProcessExit> pollExit(ProcessId processId) override {
        ++m_pollCount;
        const auto found = m_pendingExits.find(processId);
        if (found == m_pendingExits.end()) {
            return std::nullopt;
        }
        const ProcessExit exit = found->second;
        m_pendingExits.erase(found);
        return exit;
    }

    bool terminateGroup(ProcessId processId) override {
        m_terminated.push_back(processId);
        if (m_exitOnTerminate) {
            finish(processId, ProcessExit{ProcessExitKind::Signalled, SIGTERM});
        }
        return true;
    }

    bool killGroup(ProcessId processId) override {
        m_killed.push_back(processId);
        finish(processId, ProcessExit{ProcessExitKind::Signalled, SIGKILL});
        return true;
    }

    void finish(ProcessId processId, ProcessExit exit) {
        m_pendingExits[processId] = exit;
    }

    void setFailStarts(bool failStarts) {
        m_failStarts = failStarts;
    }

    void setExitOnTerminate(bool exitOnTerminate) {
        m_exitOnTerminate = exitOnTerminate;
    }

    [[nodiscard]] ProcessId lastProcessId() const {
        return m_lastProcessId;
    }

    [[nodiscard]] const std::vector<std::vector<std::string>>& startedArguments() const {
        return m_startedArguments;
    }

    [[nodiscard]] const std::vector<ProcessId>& terminated() const {
        return m_terminated;
    }

    [[nodiscard]] const std::vector<ProcessId>& killed() const {
        return m_killed;
    }

private:
    ProcessId m_nextProcessId = 1000;
    ProcessId m_lastProcessId = 0;
    bool m_failStarts = false;
    bool m_exitOnTerminate = true;
    int m_pollCount = 0;
    std::map<ProcessId, ProcessExit> m_pendingExits;
    std::vector<std::vector<std::string>> m_startedArguments;
    std::vector<ProcessId> m_terminated;
    std::vector<ProcessId> m_killed;
};

} // namespace lexus_head_unit::testing
