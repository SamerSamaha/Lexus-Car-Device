#include "emulator_process.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <csignal>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE(readability-redundant-declaration,cppcoreguidelines-avoid-non-const-global-variables)
extern char** environ;

// glibc declares POSIX symbols in internal headers; the public headers above are the right ones.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit::testing {

namespace {

constexpr int startupTimeoutMilliseconds = 10000;
constexpr int controlTimeoutMilliseconds = 5000;
std::int64_t nextInstanceNumber() {
    static std::int64_t counter = 0;
    return counter++;
}

std::string readLineWithTimeout(int fileDescriptor, int timeoutMilliseconds) {
    std::string line;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{};
        descriptor.fd = fileDescriptor;
        descriptor.events = POLLIN;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();
        if (::poll(&descriptor, 1, static_cast<int>(remaining > 0 ? remaining : 0)) <= 0) {
            break;
        }
        char character = 0;
        const ssize_t count = ::read(fileDescriptor, &character, 1);
        if (count <= 0) {
            break;
        }
        if (character == '\n') {
            return line;
        }
        line.push_back(character);
    }
    return line;
}

std::int64_t jsonInteger(const std::string& json, const std::string& key) {
    const std::string quotedKey = "\"" + key + "\"";
    const std::size_t keyPosition = json.find(quotedKey);
    if (keyPosition == std::string::npos) {
        return -1;
    }
    const std::size_t colon = json.find(':', keyPosition + quotedKey.size());
    if (colon == std::string::npos) {
        return -1;
    }
    const std::string number = json.substr(colon + 1);
    return std::strtoll(number.c_str(), nullptr, 10);
}

} // namespace

EmulatorProcess::EmulatorProcess(std::vector<std::string> extraArguments)
    : m_extraArguments(std::move(extraArguments)) {
    const std::int64_t instance = nextInstanceNumber();
    const std::string base = "/tmp/lexus_head_unit_emulator_" + std::to_string(::getpid()) + "_" +
                             std::to_string(instance);
    m_linkPath = base + ".link";
    m_controlPath = base + ".control";
}

EmulatorProcess::~EmulatorProcess() {
    kill();
    ::unlink(m_linkPath.c_str());
    ::unlink(m_controlPath.c_str());
}

std::string EmulatorProcess::repositoryRoot() {
    return LEXUS_HEAD_UNIT_REPOSITORY_ROOT;
}

bool EmulatorProcess::start() {
    if (m_pid > 0) {
        return true;
    }
    std::array<int, 2> stdoutPipe{};
    if (::pipe(stdoutPipe.data()) != 0) {
        m_lastError = "pipe failed";
        return false;
    }
    const std::string script = repositoryRoot() + "/tools/elm327_emulator/elm327_emulator.py";
    std::vector<std::string> arguments = {
        "python3", script, "--link", m_linkPath, "--control", m_controlPath};
    for (const std::string& extra : m_extraArguments) {
        arguments.push_back(extra);
    }
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, stdoutPipe.at(1), STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, stdoutPipe.at(0));
    pid_t pid = -1;
    const int spawnResult = posix_spawnp(&pid, "python3", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(stdoutPipe.at(1));
    if (spawnResult != 0) {
        ::close(stdoutPipe.at(0));
        m_lastError = std::string("posix_spawnp failed: ") + std::strerror(spawnResult);
        return false;
    }
    m_pid = pid;
    m_devicePath = readLineWithTimeout(stdoutPipe.at(0), startupTimeoutMilliseconds);
    ::close(stdoutPipe.at(0));
    if (m_devicePath.empty() || m_devicePath.rfind("/dev/", 0) != 0) {
        m_lastError = "emulator did not print a device path: '" + m_devicePath + "'";
        kill();
        return false;
    }
    // Wait for the control socket to exist.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(startupTimeoutMilliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (::access(m_controlPath.c_str(), F_OK) == 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    m_lastError = "control socket did not appear";
    kill();
    return false;
}

void EmulatorProcess::kill() {
    if (m_pid <= 0) {
        return;
    }
    ::kill(m_pid, SIGTERM);
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (::waitpid(m_pid, &status, WNOHANG) == m_pid) {
            m_pid = -1;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ::kill(m_pid, SIGKILL);
    ::waitpid(m_pid, &status, 0);
    m_pid = -1;
}

bool EmulatorProcess::isRunning() const {
    return m_pid > 0;
}

const std::string& EmulatorProcess::linkPath() const {
    return m_linkPath;
}

const std::string& EmulatorProcess::devicePath() const {
    return m_devicePath;
}

const std::string& EmulatorProcess::lastError() const {
    return m_lastError;
}

std::string EmulatorProcess::control(const std::string& command) {
    const int socketDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketDescriptor < 0) {
        return "ERR socket";
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const std::size_t copied = std::min(m_controlPath.size(), sizeof(address.sun_path) - 1);
    std::copy_n(m_controlPath.begin(), copied, std::begin(address.sun_path));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (::connect(socketDescriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(socketDescriptor);
        return "ERR connect";
    }
    const std::string line = command + "\n";
    if (::write(socketDescriptor, line.data(), line.size()) != static_cast<ssize_t>(line.size())) {
        ::close(socketDescriptor);
        return "ERR write";
    }
    const std::string reply = readLineWithTimeout(socketDescriptor, controlTimeoutMilliseconds);
    ::close(socketDescriptor);
    return reply;
}

std::int64_t EmulatorProcess::requestCount(const std::string& command) {
    const std::string statistics = control("stats");
    const std::string perCommand = "\"per_command\"";
    const std::size_t start = statistics.find(perCommand);
    if (start == std::string::npos) {
        return -1;
    }
    const std::string quotedKey = "\"" + command + "\"";
    const std::size_t keyPosition = statistics.find(quotedKey, start);
    if (keyPosition == std::string::npos) {
        return 0;
    }
    return jsonInteger(statistics.substr(keyPosition), command);
}

std::int64_t EmulatorProcess::forbiddenRequestCount() {
    return jsonInteger(control("stats"), "forbidden_requests");
}

} // namespace lexus_head_unit::testing
// NOLINTEND(misc-include-cleaner)
