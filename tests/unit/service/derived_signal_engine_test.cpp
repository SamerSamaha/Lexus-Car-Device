// Verifies: REQ-022

#include "lexus_head_unit/service/derived_signal_engine.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::DerivedSignalConstants;
using lexus_head_unit::DerivedSignalEngine;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::Unit;

SignalSample input(SignalId signalId, double value, std::int64_t timestampMilliseconds) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.value = value;
    sample.unit = lexus_head_unit::definitionOf(signalId).unit;
    sample.timestampMilliseconds = timestampMilliseconds;
    sample.status = SignalStatus::Valid;
    return sample;
}

// What a source emits: the status is left at its default and the store makes it Valid.
SignalSample fromSource(SignalId signalId, double value, std::int64_t timestampMilliseconds) {
    SignalSample sample = input(signalId, value, timestampMilliseconds);
    sample.status = SignalStatus::NeverReceived;
    return sample;
}

// The derived sample of that signal, or a NeverReceived sample when the engine gave none.
SignalSample derivedOf(const std::vector<SignalSample>& samples, SignalId signalId) {
    for (const SignalSample& sample : samples) {
        if (sample.signalId == signalId) {
            return sample;
        }
    }
    SignalSample missing;
    missing.signalId = signalId;
    missing.status = SignalStatus::NeverReceived;
    return missing;
}

bool produced(const std::vector<SignalSample>& samples, SignalId signalId) {
    return derivedOf(samples, signalId).status == SignalStatus::Valid;
}

double relativeError(double actual, double expected) {
    return std::fabs(actual - expected) / std::fabs(expected);
}

constexpr double tolerance = 0.001;
constexpr double checkValueLitresPer100Kilometres = 3.2872;

TEST(DerivedSignalFormulaTest, TenGramsPerSecondAtOneHundredKilometresPerHour) {
    const DerivedSignalConstants constants;
    // 10 g/s of air / 14.7 = 0.68027 g/s of fuel; x 3600 / 745 g/L = 3.2872 L/h.
    const double expectedLitresPerHour = 10.0 / 14.7 * 3600.0 / 745.0;
    EXPECT_LT(relativeError(lexus_head_unit::fuelLitresPerHour(10.0, constants),
                            checkValueLitresPer100Kilometres),
              tolerance);
    EXPECT_LT(relativeError(lexus_head_unit::instantFuelEconomy(10.0, 100.0, constants),
                            expectedLitresPerHour),
              tolerance);
    EXPECT_LT(relativeError(lexus_head_unit::instantFuelEconomy(10.0, 100.0, constants),
                            checkValueLitresPer100Kilometres),
              tolerance);
}

TEST(DerivedSignalEngineTest, InstantEconomyThroughTheEngineWithTheInputTimestamp) {
    DerivedSignalEngine engine;
    engine.onSample(input(SignalId::VehicleSpeed, 100.0, 1000));
    const SignalSample economy = derivedOf(
        engine.onSample(input(SignalId::MassAirFlow, 10.0, 1200)), SignalId::InstantFuelEconomy);
    ASSERT_EQ(economy.status, SignalStatus::Valid);
    EXPECT_LT(relativeError(economy.value, checkValueLitresPer100Kilometres), tolerance);
    EXPECT_EQ(economy.unit, Unit::LitresPer100Kilometres);
    EXPECT_EQ(economy.timestampMilliseconds, 1200);
}

TEST(DerivedSignalEngineTest, ConstantSpeedTraceGivesSpeedTimesTime) {
    DerivedSignalEngine engine;
    // 50 km/h for 600 s, a sample every 500 ms: 8.333 km.
    SignalSample last;
    for (std::int64_t timestamp = 0; timestamp <= 600000; timestamp += 500) {
        last = derivedOf(engine.onSample(input(SignalId::VehicleSpeed, 50.0, timestamp)),
                         SignalId::TripDistance);
    }
    ASSERT_EQ(last.status, SignalStatus::Valid);
    EXPECT_LT(relativeError(last.value, 50.0 * 600.0 / 3600.0), tolerance);
    EXPECT_EQ(last.unit, Unit::Kilometres);
    EXPECT_EQ(engine.gapsNotIntegrated(), 0U);
}

TEST(DerivedSignalEngineTest, NoInstantEconomyBelowFiveKilometresPerHourOrWithAnOldSpeed) {
    DerivedSignalEngine engine;
    engine.onSample(input(SignalId::VehicleSpeed, 4.0, 1000));
    EXPECT_FALSE(produced(engine.onSample(input(SignalId::MassAirFlow, 3.0, 1100)),
                          SignalId::InstantFuelEconomy));
    engine.onSample(input(SignalId::VehicleSpeed, 5.0, 1500));
    EXPECT_TRUE(produced(engine.onSample(input(SignalId::MassAirFlow, 3.0, 1600)),
                         SignalId::InstantFuelEconomy));
    // Speed last seen at 1500 ms; at 3501 ms it is older than 2,000 ms.
    EXPECT_FALSE(produced(engine.onSample(input(SignalId::MassAirFlow, 3.0, 3501)),
                          SignalId::InstantFuelEconomy));
}

// Drives at 72 km/h (20 m/s) with 10 g/s of air every 250 ms; returns the first time a trip
// average appeared and the last average.
std::pair<std::int64_t, SignalSample> driveTenSeconds(DerivedSignalEngine& engine) {
    std::int64_t firstAverageAt = -1;
    SignalSample average;
    for (std::int64_t timestamp = 0; timestamp <= 10000; timestamp += 250) {
        engine.onSample(input(SignalId::VehicleSpeed, 72.0, timestamp));
        const SignalSample candidate =
            derivedOf(engine.onSample(input(SignalId::MassAirFlow, 10.0, timestamp + 1)),
                      SignalId::TripFuelEconomy);
        if (candidate.status == SignalStatus::Valid) {
            average = candidate;
            firstAverageAt = firstAverageAt < 0 ? timestamp : firstAverageAt;
        }
    }
    return {firstAverageAt, average};
}

TEST(DerivedSignalEngineTest, TripAverageOnlyFromATenthOfAKilometreAndEqualToFuelOverDistance) {
    DerivedSignalEngine engine;
    const auto [firstAverageAt, average] = driveTenSeconds(engine);
    // 0.1 km is reached at 5,000 ms; summing 0.005 km steps may land one step later.
    EXPECT_GE(firstAverageAt, 5000);
    EXPECT_LE(firstAverageAt, 5250);
    ASSERT_EQ(average.status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(average.value,
                     engine.tripFuelLitres() / engine.tripDistanceKilometres() * 100.0);
    // Constant flow and speed: the average equals the instantaneous value.
    EXPECT_LT(relativeError(average.value,
                            lexus_head_unit::instantFuelEconomy(10.0, 72.0, engine.constants())),
              0.01);
}

// 2 s idle at 800, 3 s at 1,800, 4 s at 3,000, then 1 s at 4,500. Returns the last output
// and the smallest number of samples any one engine-speed sample produced.
std::pair<std::vector<SignalSample>, std::size_t>
driveThroughEveryBand(DerivedSignalEngine& engine) {
    const std::vector<std::pair<std::int64_t, double>> trace = {
        {0, 800.0}, {2000, 1800.0}, {5000, 3000.0}, {9000, 4500.0}, {10000, 4500.0}};
    std::vector<SignalSample> derived;
    std::size_t fewest = lexus_head_unit::rpmBandCount;
    for (const auto& [timestamp, rpm] : trace) {
        derived = engine.onSample(input(SignalId::EngineRpm, rpm, timestamp));
        fewest = std::min(fewest, derived.size());
    }
    return {derived, fewest};
}

TEST(DerivedSignalEngineTest, EngineSpeedBandsAccumulateFromTheEarlierSample) {
    DerivedSignalEngine engine;
    const auto [derived, emittedEachTime] = driveThroughEveryBand(engine);
    EXPECT_EQ(emittedEachTime, 4U);
    EXPECT_EQ(engine.rpmBandMilliseconds(0), 2000);
    EXPECT_EQ(engine.rpmBandMilliseconds(1), 3000);
    EXPECT_EQ(engine.rpmBandMilliseconds(2), 4000);
    EXPECT_EQ(engine.rpmBandMilliseconds(3), 1000);
    EXPECT_DOUBLE_EQ(derivedOf(derived, SignalId::Time2500To3999Rpm).value, 4000.0 / 60000.0);
    EXPECT_EQ(derivedOf(derived, SignalId::TimeFrom4000Rpm).unit, Unit::Minutes);
}

TEST(DerivedSignalEngineTest, BandBoundariesBelongToTheUpperBand) {
    DerivedSignalEngine engine;
    engine.onSample(input(SignalId::EngineRpm, 1000.0, 0));
    engine.onSample(input(SignalId::EngineRpm, 2500.0, 1000));
    engine.onSample(input(SignalId::EngineRpm, 4000.0, 2000));
    engine.onSample(input(SignalId::EngineRpm, 999.75, 3000));
    engine.onSample(input(SignalId::EngineRpm, 999.75, 4000));
    EXPECT_EQ(engine.rpmBandMilliseconds(0), 1000);
    EXPECT_EQ(engine.rpmBandMilliseconds(1), 1000);
    EXPECT_EQ(engine.rpmBandMilliseconds(2), 1000);
    EXPECT_EQ(engine.rpmBandMilliseconds(3), 1000);
}

TEST(DerivedSignalEngineTest, WarmUpTimeAppearsAtEightyDegreesAndThenStays) {
    DerivedSignalEngine engine;
    EXPECT_TRUE(engine.onSample(input(SignalId::CoolantTemperature, 20.0, 0)).empty());
    EXPECT_TRUE(engine.onSample(input(SignalId::CoolantTemperature, 79.0, 300000)).empty());
    const SignalSample reached =
        derivedOf(engine.onSample(input(SignalId::CoolantTemperature, 80.0, 360000)),
                  SignalId::CoolantWarmUpTime);
    EXPECT_EQ(reached.status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(reached.value, 6.0);
    const SignalSample later =
        derivedOf(engine.onSample(input(SignalId::CoolantTemperature, 75.0, 400000)),
                  SignalId::CoolantWarmUpTime);
    EXPECT_DOUBLE_EQ(later.value, 6.0);
    EXPECT_EQ(later.timestampMilliseconds, 400000);
}

TEST(DerivedSignalEngineTest, AWarmStartShowsZero) {
    DerivedSignalEngine engine;
    const SignalSample warmUp =
        derivedOf(engine.onSample(input(SignalId::CoolantTemperature, 88.0, 5000)),
                  SignalId::CoolantWarmUpTime);
    EXPECT_EQ(warmUp.status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(warmUp.value, 0.0);
}

TEST(DerivedSignalEngineTest, GapsOverFiveSecondsAreNotIntegratedAndAreCounted) {
    DerivedSignalEngine engine;
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 0));
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 5000));
    EXPECT_DOUBLE_EQ(engine.tripDistanceKilometres(), 0.05);
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 10001));
    EXPECT_DOUBLE_EQ(engine.tripDistanceKilometres(), 0.05);
    EXPECT_EQ(engine.gapsNotIntegrated(), 1U);
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 11001));
    EXPECT_DOUBLE_EQ(engine.tripDistanceKilometres(), 0.06);
}

TEST(DerivedSignalEngineTest, TenMinutesOfSilenceStartsANewTrip) {
    DerivedSignalEngine engine;
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 0));
    engine.onSample(input(SignalId::VehicleSpeed, 36.0, 1000));
    engine.onSample(input(SignalId::CoolantTemperature, 90.0, 1000));
    EXPECT_EQ(engine.tripsStarted(), 1U);
    EXPECT_GT(engine.tripDistanceKilometres(), 0.0);
    // Exactly 10 minutes is the same trip; one millisecond more is a new one.
    engine.onSample(input(SignalId::EngineRpm, 800.0, 601000));
    EXPECT_EQ(engine.tripsStarted(), 1U);
    engine.onSample(input(SignalId::EngineRpm, 800.0, 1201001));
    EXPECT_EQ(engine.tripsStarted(), 2U);
    EXPECT_DOUBLE_EQ(engine.tripDistanceKilometres(), 0.0);
    EXPECT_FALSE(engine.warmUpMilliseconds().has_value());
    EXPECT_EQ(engine.rpmBandMilliseconds(0), 0);
}

TEST(DerivedSignalEngineTest, OtherSignalsStaleSamplesAndDerivedSamplesGiveNothing) {
    DerivedSignalEngine engine;
    EXPECT_TRUE(engine.onSample(input(SignalId::FuelLevel, 50.0, 0)).empty());
    EXPECT_TRUE(engine.onSample(input(SignalId::TripDistance, 1.0, 0)).empty());
    SignalSample stale = input(SignalId::VehicleSpeed, 50.0, 0);
    stale.status = SignalStatus::Stale;
    EXPECT_TRUE(engine.onSample(stale).empty());
    EXPECT_EQ(engine.tripsStarted(), 0U);
}

TEST(DerivedSignalEngineTest, EveryDerivedSignalIsProducedByAnInput) {
    DerivedSignalEngine engine;
    std::vector<SignalSample> all;
    const std::vector<SignalSample> inputs = {
        input(SignalId::VehicleSpeed, 72.0, 0),
        input(SignalId::EngineRpm, 2000.0, 0),
        input(SignalId::CoolantTemperature, 90.0, 0),
        input(SignalId::VehicleSpeed, 72.0, 4999),
        input(SignalId::VehicleSpeed, 72.0, 6000),
        input(SignalId::MassAirFlow, 10.0, 6000),
    };
    for (const SignalSample& sample : inputs) {
        const std::vector<SignalSample> output = engine.onSample(sample);
        all.insert(all.end(), output.begin(), output.end());
    }
    for (const SignalId signalId : lexus_head_unit::derivedSignalIds) {
        EXPECT_TRUE(produced(all, signalId)) << lexus_head_unit::toString(signalId);
    }
}

TEST(DerivedSignalFeederTest, DerivedSamplesReachTheStoreAndItsListenerWithTheInputTimestamp) {
    lexus_head_unit::SignalStore store;
    std::size_t published = 0;
    store.setChangeListener([&published](const SignalSample& /*sample*/) {
        ++published;
    });
    lexus_head_unit::SignalStoreFeeder feeder(store);
    feeder.onSample(fromSource(SignalId::EngineRpm, 800.0, 1000));
    feeder.onSample(fromSource(SignalId::EngineRpm, 800.0, 2000));
    EXPECT_EQ(feeder.acceptedSampleCount(), 2U);
    EXPECT_EQ(feeder.derivedSampleCount(), 8U);
    EXPECT_EQ(published, 10U);
    const SignalSample& idle = store.latest(SignalId::TimeBelow1000Rpm);
    EXPECT_EQ(idle.status, SignalStatus::Valid);
    EXPECT_EQ(idle.timestampMilliseconds, 2000);
    EXPECT_DOUBLE_EQ(idle.value, 1000.0 / 60000.0);
}

TEST(DerivedSignalFeederTest, ARejectedInputProducesNoDerivedSample) {
    lexus_head_unit::SignalStore store;
    lexus_head_unit::SignalStoreFeeder feeder(store);
    feeder.onSample(fromSource(SignalId::VehicleSpeed, 50.0, 2000));
    feeder.onSample(fromSource(SignalId::VehicleSpeed, 50.0, 1000));
    EXPECT_EQ(feeder.rejectedSampleCount(), 1U);
    EXPECT_EQ(feeder.derivedSampleCount(), 1U);
    EXPECT_DOUBLE_EQ(feeder.derivedSignalEngine().tripDistanceKilometres(), 0.0);
}

} // namespace
