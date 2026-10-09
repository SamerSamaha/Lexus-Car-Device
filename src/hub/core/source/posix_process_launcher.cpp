#include "lexus_head_unit/hub/posix_process_launcher.h"

#include "lexus_head_unit/hub/process_launcher.h"

#include <cerrno>
#include <csignal>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE(readability-redundant-declaration,cppcoreguidelines-avoid-non-const-global-variables)
extern char** environ;

// glibc declares the POSIX symbols in internal headers; the public headers above are the
// right ones to include.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

// posix_spawnp takes char* const[]; each argument is copied into its own writable buffer so
// that no const is cast away.
std::vector<std::vector<char>> writableCopies(const std::vector<std::string>& arguments) {
    std::vector<std::vector<char>> copies;
    copies.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        std::vector<char> buffer(argument.begin(), argument.end());
        buffer.push_back('\0');
        copies.push_back(std::move(buffer));
    }
    return copies;
}

bool signalGroup(ProcessId processId, int signalNumber) {
    if (processId <= 0) {
        return false;
    }
    return ::kill(-static_cast<pid_t>(processId), signalNumber) == 0;
}

} // namespace

std::optional<ProcessId> PosixProcessLauncher::start(const std::vector<std::string>& arguments) {
    if (arguments.empty() || arguments.front().empty()) {
        return std::nullopt;
    }
    std::vector<std::vector<char>> copies = writableCopies(arguments);
    std::vector<char*> argumentPointers;
    argumentPointers.reserve(copies.size() + 1);
    for (std::vector<char>& copy : copies) {
        argumentPointers.push_back(copy.data());
    }
    argumentPointers.push_back(nullptr);

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    // A new process group (ID = the child's process ID), default signal handlers and an empty
    // signal mask in the child, whatever the hub itself has set.
    sigset_t defaultSignals;
    sigfillset(&defaultSignals);
    sigset_t emptyMask;
    sigemptyset(&emptyMask);
    posix_spawnattr_setsigdefault(&attributes, &defaultSignals);
    posix_spawnattr_setsigmask(&attributes, &emptyMask);
    posix_spawnattr_setpgroup(&attributes, 0);
    posix_spawnattr_setflags(
        &attributes,
        static_cast<short>(POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK));

    pid_t childProcessId = 0;
    const int result = posix_spawnp(&childProcessId,
                                    argumentPointers.front(),
                                    nullptr,
                                    &attributes,
                                    argumentPointers.data(),
                                    environ);
    posix_spawnattr_destroy(&attributes);
    if (result != 0) {
        return std::nullopt;
    }
    return static_cast<ProcessId>(childProcessId);
}

std::optional<ProcessExit> PosixProcessLauncher::pollExit(ProcessId processId) {
    int status = 0;
    const pid_t waited = ::waitpid(static_cast<pid_t>(processId), &status, WNOHANG);
    if (waited == 0) {
        return std::nullopt;
    }
    if (waited < 0) {
        if (errno == EINTR) {
            return std::nullopt;
        }
        return ProcessExit{ProcessExitKind::Lost, 0};
    }
    if (WIFEXITED(status)) {
        return ProcessExit{ProcessExitKind::Exited, WEXITSTATUS(status)};
    }
    if (WIFSIGNALED(status)) {
        return ProcessExit{ProcessExitKind::Signalled, WTERMSIG(status)};
    }
    return std::nullopt;
}

bool PosixProcessLauncher::terminateGroup(ProcessId processId) {
    return signalGroup(processId, SIGTERM);
}

bool PosixProcessLauncher::killGroup(ProcessId processId) {
    return signalGroup(processId, SIGKILL);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
