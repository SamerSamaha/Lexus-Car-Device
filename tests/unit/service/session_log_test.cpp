// The session log is the tool of the REQ-019 parked session (LHU-024); REQ-019 itself is
// verified in the car (docs/test/MANUAL_ON_CAR_PROCEDURE.md), so this file carries no tag.

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/session_log.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::SessionLog;
using lexus_head_unit::SourceCounters;

std::vector<std::string> linesOf(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

TEST(SessionLogTest, TransitionsAndCountersAtTheInterval) {
    const std::string path = "/tmp/lexus_session_log_" + std::to_string(::getpid()) + ".csv";
    {
        SessionLog log(path, 10000);
        ASSERT_TRUE(log.isOpen());
        log.recordTransition(ConnectionTransition{ConnectionState::Disconnected,
                                                  ConnectionTrigger::StartRequested,
                                                  ConnectionState::Connecting,
                                                  100});
        SourceCounters counters;
        counters.requestsSent = 12;
        counters.samplesEmitted = 10;
        counters.malformedInputs = 1;
        EXPECT_TRUE(log.recordCountersIfDue(200, ConnectionState::Connected, counters));
        EXPECT_FALSE(log.recordCountersIfDue(10199, ConnectionState::Connected, counters));
        counters.requestsSent = 300;
        EXPECT_TRUE(log.recordCountersIfDue(10200, ConnectionState::Connected, counters));
        log.recordTransition(ConnectionTransition{ConnectionState::Connected,
                                                  ConnectionTrigger::LinkLost,
                                                  ConnectionState::Error,
                                                  15000});
        EXPECT_EQ(log.rowsWritten(), 4U);
    }
    const std::vector<std::string> lines = linesOf(path);
    static_cast<void>(std::remove(path.c_str()));
    const std::vector<std::string> expected = {
        SessionLog::header,
        "100,transition,Connecting,StartRequested,Disconnected,Connecting,,,",
        "200,counters,Connected,,,,12,10,1",
        "10200,counters,Connected,,,,300,10,1",
        "15000,transition,Error,LinkLost,Connected,Error,,,",
    };
    EXPECT_EQ(lines, expected);
}

TEST(SessionLogTest, AnUnwritablePathIsNotOpenAndWritesNothing) {
    SessionLog log("/nonexistent/folder/session.csv", 1000);
    EXPECT_FALSE(log.isOpen());
    log.recordTransition(ConnectionTransition{});
    EXPECT_EQ(log.rowsWritten(), 0U);
}

} // namespace
