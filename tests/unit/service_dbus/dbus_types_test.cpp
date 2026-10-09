// Verifies: REQ-017

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service_dbus/dbus_names.h"
#include "lexus_head_unit/service_dbus/dbus_types.h"

#include <gtest/gtest.h>

#include <QtGlobal>

#include <cstdint>
#include <optional>
#include <tuple>

// Qt declares its types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::allConnectionStates;
using lexus_head_unit::allConnectionTriggers;
using lexus_head_unit::allSignalIds;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::DBusSample;
using lexus_head_unit::definitionOf;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;

auto fieldsOf(const SignalSample& sample) {
    return std::make_tuple(
        sample.signalId, sample.value, sample.unit, sample.timestampMilliseconds, sample.status);
}

auto fieldsOf(const ConnectionTransition& transition) {
    return std::make_tuple(
        transition.from, transition.trigger, transition.to, transition.timestampMilliseconds);
}

TEST(DBusTypesTest, EverySignalAndStatusRoundTrips) {
    for (const auto signalId : allSignalIds) {
        for (const auto status :
             {SignalStatus::NeverReceived, SignalStatus::Valid, SignalStatus::Stale}) {
            SignalSample sample;
            sample.signalId = signalId;
            sample.value = -12.75;
            sample.unit = definitionOf(signalId).unit;
            sample.timestampMilliseconds = std::int64_t{9007199254740993};
            sample.status = status;
            const std::optional<SignalSample> back =
                lexus_head_unit::fromDBus(lexus_head_unit::toDBus(sample));
            EXPECT_EQ(fieldsOf(back.value_or(SignalSample{})), fieldsOf(sample));
        }
    }
}

TEST(DBusTypesTest, OutOfRangeNumbersAreRejected) {
    const DBusSample good{0, 1.0, 0, 5, 1};
    EXPECT_TRUE(lexus_head_unit::fromDBus(good).has_value());
    EXPECT_FALSE(lexus_head_unit::fromDBus(DBusSample{8, 1.0, 0, 5, 1}).has_value());
    EXPECT_FALSE(lexus_head_unit::fromDBus(DBusSample{0, 1.0, 5, 5, 1}).has_value());
    EXPECT_FALSE(lexus_head_unit::fromDBus(DBusSample{0, 1.0, 0, 5, 3}).has_value());
    EXPECT_FALSE(lexus_head_unit::connectionStateFromDBus(4).has_value());
    EXPECT_FALSE(lexus_head_unit::transitionFromDBus(0, 6, 1, 0).has_value());
    EXPECT_FALSE(lexus_head_unit::transitionFromDBus(4, 0, 1, 0).has_value());
    EXPECT_FALSE(lexus_head_unit::transitionFromDBus(0, 0, 4, 0).has_value());
}

// Wire form and back for one transition: the state, the flag and every field survive.
bool roundTrips(const ConnectionTransition& transition) {
    const auto wire = lexus_head_unit::toDBus(transition.to, transition);
    const auto back = lexus_head_unit::transitionFromDBus(
        wire.from, wire.trigger, wire.to, wire.timestampMilliseconds);
    return wire.hasTransition &&
           lexus_head_unit::connectionStateFromDBus(wire.state) ==
               std::optional<ConnectionState>(transition.to) &&
           fieldsOf(back.value_or(ConnectionTransition{})) == fieldsOf(transition);
}

TEST(DBusTypesTest, EveryStateAndTriggerRoundTrips) {
    int roundTripCount = 0;
    for (const auto from : allConnectionStates) {
        for (const auto trigger : allConnectionTriggers) {
            ConnectionTransition transition;
            transition.from = from;
            transition.trigger = trigger;
            transition.to = ConnectionState::Error;
            transition.timestampMilliseconds = 42;
            roundTripCount += roundTrips(transition) ? 1 : 0;
        }
    }
    EXPECT_EQ(roundTripCount, 24);
    const auto noTransition = lexus_head_unit::toDBus(ConnectionState::Disconnected, std::nullopt);
    EXPECT_FALSE(noTransition.hasTransition);
    EXPECT_EQ(noTransition.state, 0U);
}

TEST(DBusTypesTest, NamesCarryTheInterfaceVersion) {
    EXPECT_TRUE(lexus_head_unit::dbus_names::interfaceName().endsWith(QStringLiteral("1")));
    EXPECT_EQ(lexus_head_unit::dbus_names::interfaceVersion, 1U);
    EXPECT_TRUE(lexus_head_unit::dbus_names::objectPath().startsWith(QStringLiteral("/")));
}

} // namespace
// NOLINTEND(misc-include-cleaner)
