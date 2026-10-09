#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lexus_head_unit {

using ProcessId = std::int64_t;

enum class ProcessExitKind {
    Exited,
    Signalled,
    // The process could not be waited for any more (it was reaped elsewhere).
    Lost,
};

struct ProcessExit {
    ProcessExitKind kind = ProcessExitKind::Exited;
    // The exit status for Exited, the signal number for Signalled, 0 for Lost.
    int code = 0;
};

// Starts and watches child processes. Each process is started in a new process group whose ID
// equals its process ID, so that a signal can reach the process and every child it started.
class ProcessLauncher {
public:
    ProcessLauncher() = default;
    ProcessLauncher(const ProcessLauncher&) = delete;
    ProcessLauncher& operator=(const ProcessLauncher&) = delete;
    ProcessLauncher(ProcessLauncher&&) = delete;
    ProcessLauncher& operator=(ProcessLauncher&&) = delete;
    virtual ~ProcessLauncher() = default;

    // Starts arguments[0] (searched on PATH) with the rest as its arguments. Empty when the
    // program cannot be started.
    virtual std::optional<ProcessId> start(const std::vector<std::string>& arguments) = 0;
    // Does not block. Empty while the process runs; once it has ended, its exit, after which
    // the process is reaped and must not be polled again.
    virtual std::optional<ProcessExit> pollExit(ProcessId processId) = 0;
    // SIGTERM and SIGKILL to the process group. False if the group no longer exists.
    virtual bool terminateGroup(ProcessId processId) = 0;
    virtual bool killGroup(ProcessId processId) = 0;
};

} // namespace lexus_head_unit
