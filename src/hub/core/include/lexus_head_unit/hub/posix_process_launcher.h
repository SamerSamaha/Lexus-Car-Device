#pragma once

#include "lexus_head_unit/hub/process_launcher.h"

#include <optional>
#include <string>
#include <vector>

namespace lexus_head_unit {

// ProcessLauncher over posix_spawnp, waitpid(WNOHANG) on one process ID, and kill on the group.
class PosixProcessLauncher final : public ProcessLauncher {
public:
    std::optional<ProcessId> start(const std::vector<std::string>& arguments) override;
    std::optional<ProcessExit> pollExit(ProcessId processId) override;
    bool terminateGroup(ProcessId processId) override;
    bool killGroup(ProcessId processId) override;
};

} // namespace lexus_head_unit
