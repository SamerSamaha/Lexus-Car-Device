#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <sys/types.h>

namespace lexus_head_unit::testing {

// Spawns tools/elm327_emulator/elm327_emulator.py, reads the device path it prints, talks to
// its control socket, and can kill and restart it at the same link path.
class EmulatorProcess {
public:
    explicit EmulatorProcess(std::vector<std::string> extraArguments = {});
    ~EmulatorProcess();

    EmulatorProcess(const EmulatorProcess&) = delete;
    EmulatorProcess& operator=(const EmulatorProcess&) = delete;
    EmulatorProcess(EmulatorProcess&&) = delete;
    EmulatorProcess& operator=(EmulatorProcess&&) = delete;

    // Starts the process; returns false with the reason in lastError() if it could not.
    bool start();
    void kill();
    [[nodiscard]] bool isRunning() const;

    [[nodiscard]] const std::string& linkPath() const;
    [[nodiscard]] const std::string& devicePath() const;
    [[nodiscard]] const std::string& lastError() const;

    // One control command; returns the reply line (OK, ERR ..., or JSON for stats).
    std::string control(const std::string& command);
    // The per-command request count from "stats", or -1 if unavailable.
    std::int64_t requestCount(const std::string& command);
    std::int64_t forbiddenRequestCount();

    static std::string repositoryRoot();

private:
    std::vector<std::string> m_extraArguments;
    std::string m_linkPath;
    std::string m_controlPath;
    std::string m_devicePath;
    std::string m_lastError;
    pid_t m_pid = -1;
};

} // namespace lexus_head_unit::testing
