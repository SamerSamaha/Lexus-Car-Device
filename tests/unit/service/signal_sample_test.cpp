// Verifies: REQ-003, REQ-006

#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <gtest/gtest.h>

#include <set>
#include <string>

namespace {

using lexus_head_unit::allSignalIds;
using lexus_head_unit::defaultSignalDefinitions;
using lexus_head_unit::defaultStalenessTimeoutMilliseconds;
using lexus_head_unit::definitionOf;
using lexus_head_unit::indexOf;
using lexus_head_unit::signalCount;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::toString;
using lexus_head_unit::Unit;

TEST(SignalSampleTest, DefaultSampleIsNeverReceived) {
    const SignalSample sample;
    EXPECT_EQ(sample.status, SignalStatus::NeverReceived);
    EXPECT_EQ(sample.value, 0.0);
    EXPECT_EQ(sample.timestampMilliseconds, 0);
}

TEST(SignalSampleTest, EveryFieldRoundTrips) {
    SignalSample sample;
    sample.signalId = SignalId::CoolantTemperature;
    sample.value = 87.5;
    sample.unit = Unit::DegreesCelsius;
    sample.timestampMilliseconds = 1234567;
    sample.status = SignalStatus::Valid;

    const SignalSample copy = sample;
    EXPECT_EQ(copy.signalId, SignalId::CoolantTemperature);
    EXPECT_DOUBLE_EQ(copy.value, 87.5);
    EXPECT_EQ(copy.unit, Unit::DegreesCelsius);
    EXPECT_EQ(copy.timestampMilliseconds, 1234567);
    EXPECT_EQ(copy.status, SignalStatus::Valid);
}

TEST(SignalSampleTest, TimestampHoldsOneMillisecondResolution) {
    SignalSample earlier;
    earlier.timestampMilliseconds = 1000;
    SignalSample later;
    later.timestampMilliseconds = 1001;
    EXPECT_LT(earlier.timestampMilliseconds, later.timestampMilliseconds);
}

TEST(SignalSampleTest, SignalIdTextIsDistinctForEveryId) {
    std::set<std::string> names;
    for (const SignalId signalId : allSignalIds) {
        const std::string name(toString(signalId));
        EXPECT_FALSE(name.empty());
        EXPECT_NE(name, "UnknownSignal");
        names.insert(name);
    }
    EXPECT_EQ(names.size(), signalCount);
}

TEST(SignalSampleTest, StatusAndUnitTextAreNamed) {
    EXPECT_EQ(toString(SignalStatus::NeverReceived), "NeverReceived");
    EXPECT_EQ(toString(SignalStatus::Valid), "Valid");
    EXPECT_EQ(toString(SignalStatus::Stale), "Stale");
    EXPECT_EQ(toString(Unit::KilometresPerHour), "km/h");
    EXPECT_EQ(toString(Unit::RevolutionsPerMinute), "rpm");
    EXPECT_EQ(toString(Unit::DegreesCelsius), "degC");
    EXPECT_EQ(toString(Unit::Percent), "%");
    EXPECT_EQ(toString(Unit::Volts), "V");
}

TEST(SignalDefinitionTest, TableHasOneRowPerSignalInEnumerationOrder) {
    const auto& table = defaultSignalDefinitions();
    ASSERT_EQ(table.size(), signalCount);
    for (const SignalId signalId : allSignalIds) {
        EXPECT_EQ(table.at(indexOf(signalId)).id, signalId);
        EXPECT_FALSE(table.at(indexOf(signalId)).name.empty());
    }
}

TEST(SignalDefinitionTest, EveryDefaultTimeoutIsOneSecond) {
    for (const auto& definition : defaultSignalDefinitions()) {
        EXPECT_EQ(definition.stalenessTimeoutMilliseconds, defaultStalenessTimeoutMilliseconds);
    }
    EXPECT_EQ(defaultStalenessTimeoutMilliseconds, 1000);
}

TEST(SignalDefinitionTest, DefinitionOfReturnsTheMatchingRowWithItsUnit) {
    EXPECT_EQ(definitionOf(SignalId::VehicleSpeed).unit, Unit::KilometresPerHour);
    EXPECT_EQ(definitionOf(SignalId::EngineRpm).unit, Unit::RevolutionsPerMinute);
    EXPECT_EQ(definitionOf(SignalId::CoolantTemperature).unit, Unit::DegreesCelsius);
    EXPECT_EQ(definitionOf(SignalId::EngineLoad).unit, Unit::Percent);
    EXPECT_EQ(definitionOf(SignalId::ThrottlePosition).unit, Unit::Percent);
    EXPECT_EQ(definitionOf(SignalId::IntakeAirTemperature).unit, Unit::DegreesCelsius);
    EXPECT_EQ(definitionOf(SignalId::ControlModuleVoltage).unit, Unit::Volts);
    EXPECT_EQ(definitionOf(SignalId::FuelLevel).unit, Unit::Percent);
}

} // namespace
