// Verifies: REQ-016

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/posix_process_launcher.h"
#include "lexus_head_unit/service/clock.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// glibc declares the POSIX symbols in internal headers; the public headers above are the
// right ones to include.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::AppEntry;
using lexus_head_unit::AppExitCause;
using lexus_head_unit::AppExitReport;
using lexus_head_unit::AppManagerSettings;
using lexus_head_unit::AppProcessManager;
using lexus_head_unit::AppRunState;
using lexus_head_unit::LaunchResult;
using lexus_head_unit::PosixProcessLauncher;
using lexus_head_unit::SteadyClock;

constexpr int cycleCount = 20;
constexpr std::int64_t appRunMilliseconds = 50;
constexpr std::int64_t exitBudgetMilliseconds = 1000;
constexpr std::int64_t pollIntervalMilliseconds = 100;

AppEntry fakeApp(const std::vector<std::string>& options) {
    AppEntry entry;
    entry.id = "fake";
    entry.name = "Fake";
    entry.arguments = {LEXUS_HEAD_UNIT_FAKE_APP_PATH};
    entry.arguments.insert(entry.arguments.end(), options.begin(), options.end());
    return entry;
}

// Polls the way the hub does (every 100 ms) until the manager is idle; returns the elapsed time.
std::int64_t pollUntilIdle(AppProcessManager& manager, std::int64_t limitMilliseconds) {
    const auto start = std::chrono::steady_clock::now();
    const auto elapsed = [&start]() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };
    while (manager.runState() != AppRunState::Idle && elapsed() < limitMilliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMilliseconds));
        manager.poll();
    }
    return elapsed();
}

// Alive means present and not a zombie: a killed process that nobody reaps (a container whose
// first process does not reap) stays in the table in state Z.
// One launch and exit, the way the hub does it; returns the milliseconds from launch until the
// exit was seen.
std::int64_t
runOneCycle(AppProcessManager& manager, const AppEntry& entry, pid_t hubProcessId, int cycle) {
    EXPECT_EQ(manager.launch(entry), LaunchResult::Started) << "cycle " << cycle;
    EXPECT_NE(manager.foregroundProcessId().value_or(hubProcessId), hubProcessId);
    const std::int64_t elapsed =
        pollUntilIdle(manager, appRunMilliseconds + exitBudgetMilliseconds + 500);
    EXPECT_EQ(manager.runState(), AppRunState::Idle) << "cycle " << cycle;
    EXPECT_LE(elapsed, appRunMilliseconds + exitBudgetMilliseconds) << "cycle " << cycle;
    EXPECT_EQ(::getpid(), hubProcessId);
    return elapsed;
}

// The raw per-cycle times, printed so that the CI log keeps the measurement.
void printSummary(const std::string& what, std::vector<std::int64_t> values) {
    std::sort(values.begin(), values.end());
    std::cout << what << " (ms, " << values.size() << " cycles): min " << values.front()
              << " median " << values.at(values.size() / 2) << " max " << values.back() << "\n";
}

AppExitReport lastExitOf(const AppProcessManager& manager) {
    return manager.lastExit().value_or(AppExitReport{});
}

pid_t waitForChildProcessId(const std::string& childFile) {
    pid_t childProcessId = 0;
    for (int attempt = 0; attempt < 100 && childProcessId == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::ifstream file(childFile);
        file >> childProcessId;
    }
    return childProcessId;
}

bool processIsAlive(pid_t processId);

// True once the process has gone (or is a zombie), checked every 10 ms for up to 1 s.
bool waitUntilGone(pid_t processId) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!processIsAlive(processId)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool processIsAlive(pid_t processId) {
    std::ifstream stat("/proc/" + std::to_string(processId) + "/stat");
    std::string text;
    if (!std::getline(stat, text)) {
        return false;
    }
    const std::size_t closing = text.rfind(')');
    return closing != std::string::npos && closing + 2 < text.size() && text.at(closing + 2) != 'Z';
}

TEST(HubProcessCycleTest, TwentyStartAndExitCyclesEachSeenWithinOneSecond) {
    const SteadyClock clock;
    PosixProcessLauncher launcher;
    AppProcessManager manager(launcher, clock);
    std::vector<AppExitReport> reports;
    manager.setExitListener([&reports](const AppExitReport& exitReport) {
        reports.push_back(exitReport);
    });
    const pid_t hubProcessId = ::getpid();
    const AppEntry entry = fakeApp({"--run-ms", std::to_string(appRunMilliseconds)});
    std::vector<std::int64_t> elapsedPerCycle;
    elapsedPerCycle.reserve(cycleCount);
    for (int cycle = 0; cycle < cycleCount; ++cycle) {
        elapsedPerCycle.push_back(runOneCycle(manager, entry, hubProcessId, cycle));
    }
    printSummary("exit seen after launch", elapsedPerCycle);
    ASSERT_EQ(reports.size(), static_cast<std::size_t>(cycleCount));
    EXPECT_TRUE(std::all_of(reports.begin(), reports.end(), [](const AppExitReport& exitReport) {
        return exitReport.cause == AppExitCause::CleanExit;
    }));
    EXPECT_EQ(manager.counters().launches, static_cast<std::uint64_t>(cycleCount));
    EXPECT_EQ(manager.counters().exits, static_cast<std::uint64_t>(cycleCount));
    // Every child was reaped: there is nothing left to wait for.
    EXPECT_EQ(::waitpid(-1, nullptr, WNOHANG), -1);
    EXPECT_EQ(errno, ECHILD);
}

TEST(HubProcessCycleTest, FailedExitAndMissingProgramAreReported) {
    const SteadyClock clock;
    PosixProcessLauncher launcher;
    AppProcessManager manager(launcher, clock);
    ASSERT_EQ(manager.launch(fakeApp({"--run-ms", "10", "--exit-code", "7"})),
              LaunchResult::Started);
    pollUntilIdle(manager, 2000);
    ASSERT_TRUE(manager.lastExit().has_value());
    EXPECT_EQ(lastExitOf(manager).cause, AppExitCause::FailedExit);
    EXPECT_EQ(lastExitOf(manager).code, 7);

    AppEntry missing = fakeApp({});
    missing.arguments = {"/nonexistent/lexus-test-program"};
    EXPECT_EQ(manager.launch(missing), LaunchResult::StartFailed);
    EXPECT_EQ(lastExitOf(manager).cause, AppExitCause::StartFailed);
}

TEST(HubProcessCycleTest, StopEndsTheWholeProcessGroupAndKillsAnAppThatIgnoresTerm) {
    const SteadyClock clock;
    PosixProcessLauncher launcher;
    AppManagerSettings settings;
    settings.stopGraceMilliseconds = 300;
    AppProcessManager manager(launcher, clock, settings);
    const std::string childFile =
        "/tmp/lexus_hub_test_child_" + std::to_string(::getpid()) + ".txt";
    ASSERT_EQ(manager.launch(fakeApp({"--ignore-term", "--child-pid-file", childFile})),
              LaunchResult::Started);
    const pid_t childProcessId = waitForChildProcessId(childFile);
    ASSERT_GT(childProcessId, 0);
    ASSERT_TRUE(manager.requestStop());
    pollUntilIdle(manager, 3000);
    EXPECT_EQ(std::make_tuple(manager.runState(),
                              manager.counters().kills,
                              lastExitOf(manager).cause,
                              lastExitOf(manager).code),
              std::make_tuple(
                  AppRunState::Idle, std::uint64_t{1}, AppExitCause::StoppedByHub, int{SIGKILL}));
    // The grandchild was in the app's process group, so the same SIGKILL ended it; it is
    // reparented away from this process, so only its absence can be checked.
    EXPECT_TRUE(waitUntilGone(childProcessId));
    static_cast<void>(std::remove(childFile.c_str()));
}

} // namespace
// NOLINTEND(misc-include-cleaner)
