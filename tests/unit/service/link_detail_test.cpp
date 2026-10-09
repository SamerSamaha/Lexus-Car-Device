// Verifies: REQ-023, REQ-007

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace {

using lexus_head_unit::allLinkDetails;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::LinkDetail;
using lexus_head_unit::linkDetailAfter;
using lexus_head_unit::linkDetailCount;
using lexus_head_unit::linkDetailForState;
using lexus_head_unit::linkDetailFromNumber;

ConnectionTransition transitionTo(ConnectionState target, ConnectionTrigger trigger) {
    ConnectionTransition transition;
    transition.to = target;
    transition.trigger = trigger;
    return transition;
}

TEST(LinkDetailTest, EachStateMapsToOneDetail) {
    EXPECT_EQ(linkDetailForState(ConnectionState::Disconnected), LinkDetail::Idle);
    EXPECT_EQ(linkDetailForState(ConnectionState::Connecting), LinkDetail::SearchingForAdapter);
    EXPECT_EQ(linkDetailForState(ConnectionState::Connected), LinkDetail::Live);
    EXPECT_EQ(linkDetailForState(ConnectionState::Error), LinkDetail::LinkLostRetrying);
}

TEST(LinkDetailTest, AFailedHandshakeIsSearchingAndALostLinkIsRetrying) {
    EXPECT_EQ(
        linkDetailAfter(transitionTo(ConnectionState::Error, ConnectionTrigger::HandshakeFailed),
                        LinkDetail::Live),
        LinkDetail::SearchingForAdapter);
    EXPECT_EQ(linkDetailAfter(transitionTo(ConnectionState::Error, ConnectionTrigger::LinkLost),
                              LinkDetail::Live),
              LinkDetail::LinkLostRetrying);
    EXPECT_EQ(linkDetailAfter(
                  transitionTo(ConnectionState::Connected, ConnectionTrigger::HandshakeSucceeded),
                  LinkDetail::AdapterWithoutVehicle),
              LinkDetail::Live);
    EXPECT_EQ(linkDetailAfter(
                  transitionTo(ConnectionState::Disconnected, ConnectionTrigger::StopRequested),
                  LinkDetail::Live),
              LinkDetail::Idle);
}

TEST(LinkDetailTest, ConnectingKeepsThePreviousDetailExceptAtTheStart) {
    EXPECT_EQ(linkDetailAfter(
                  transitionTo(ConnectionState::Connecting, ConnectionTrigger::BackoffElapsed),
                  LinkDetail::AdapterWithoutVehicle),
              LinkDetail::AdapterWithoutVehicle);
    EXPECT_EQ(linkDetailAfter(
                  transitionTo(ConnectionState::Connecting, ConnectionTrigger::BackoffElapsed),
                  LinkDetail::LinkLostRetrying),
              LinkDetail::LinkLostRetrying);
    EXPECT_EQ(linkDetailAfter(
                  transitionTo(ConnectionState::Connecting, ConnectionTrigger::StartRequested),
                  LinkDetail::Idle),
              LinkDetail::SearchingForAdapter);
}

TEST(LinkDetailTest, NumbersOnTheWireRoundTripAndOutOfRangeIsRejected) {
    for (const LinkDetail detail : allLinkDetails) {
        EXPECT_EQ(linkDetailFromNumber(static_cast<unsigned int>(detail)), detail);
    }
    EXPECT_EQ(linkDetailFromNumber(static_cast<unsigned int>(linkDetailCount)), std::nullopt);
    EXPECT_EQ(linkDetailFromNumber(4096U), std::nullopt);
}

// The longest strip text, in characters; zero when one is empty.
std::size_t longestNonEmptyText() {
    std::size_t longest = 0;
    for (const LinkDetail detail : allLinkDetails) {
        const std::size_t length = displayText(detail).size();
        if (length == 0) {
            return 0;
        }
        longest = std::max(longest, length);
    }
    return longest;
}

TEST(LinkDetailTest, EveryDetailHasADistinctNameAndStripText) {
    std::set<std::string> names;
    std::set<std::string> texts;
    for (const LinkDetail detail : allLinkDetails) {
        names.insert(std::string(toString(detail)));
        texts.insert(std::string(displayText(detail)));
    }
    EXPECT_EQ(names.size(), linkDetailCount);
    EXPECT_EQ(texts.size(), linkDetailCount);
    // Not empty, and one line on the strip beside the other items (DN-042).
    EXPECT_GT(longestNonEmptyText(), 0U);
    EXPECT_LE(longestNonEmptyText(), 26U);
}

TEST(LinkDetailTest, StripTextsAreTheWordsADriverReads) {
    EXPECT_EQ(displayText(LinkDetail::SearchingForAdapter), "Searching for adapter");
    EXPECT_EQ(displayText(LinkDetail::AdapterWithoutVehicle), "Adapter found, no vehicle");
    EXPECT_EQ(displayText(LinkDetail::Live), "Live");
    EXPECT_EQ(displayText(LinkDetail::LinkLostRetrying), "Link lost, retrying");
}

} // namespace
