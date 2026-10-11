// Verifies: REQ-027

#include "lexus_head_unit/hmi/display_units.h"
#include "lexus_head_unit/hmi/signal_tile_model.h"
#include "lexus_head_unit/hmi/vehicle_data_view_model.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QString>

#include <gtest/gtest.h>

#include <optional>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::chooseDisplayUnits;
using lexus_head_unit::DisplayUnits;
using lexus_head_unit::displayUnitText;
using lexus_head_unit::displayValue;
using lexus_head_unit::parseDisplayUnits;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalTileModel;
using lexus_head_unit::Unit;
using lexus_head_unit::VehicleDataViewModel;

constexpr double tolerance = 1e-9;

double usValue(double metric, Unit unit) {
    const std::optional<double> shown = displayValue(metric, unit, DisplayUnits::UsCustomary);
    EXPECT_TRUE(shown.has_value());
    return shown.value_or(0.0);
}

TEST(DisplayUnitsTest, ConversionsUseTheExactFactors) {
    EXPECT_NEAR(usValue(100.0, Unit::KilometresPerHour), 62.137119223733397, tolerance);
    EXPECT_NEAR(usValue(1.609344, Unit::Kilometres), 1.0, tolerance);
    EXPECT_NEAR(usValue(0.0, Unit::DegreesCelsius), 32.0, tolerance);
    EXPECT_NEAR(usValue(100.0, Unit::DegreesCelsius), 212.0, tolerance);
    EXPECT_NEAR(usValue(-40.0, Unit::DegreesCelsius), -40.0, tolerance);
    // 235.2145833 / L per 100 km: 10 L/100 km is 23.52 mpg.
    EXPECT_NEAR(usValue(10.0, Unit::LitresPer100Kilometres), 23.521458333333333, tolerance);
    EXPECT_NEAR(usValue(28.5, Unit::LitresPer100Kilometres), 8.2531432748538, 1e-9);
    // Units without a US form are unchanged.
    EXPECT_DOUBLE_EQ(usValue(988.0, Unit::RevolutionsPerMinute), 988.0);
    EXPECT_DOUBLE_EQ(usValue(13.8, Unit::Volts), 13.8);
    EXPECT_DOUBLE_EQ(usValue(30.0, Unit::Percent), 30.0);
    EXPECT_DOUBLE_EQ(usValue(5.7, Unit::Minutes), 5.7);
    EXPECT_DOUBLE_EQ(usValue(12.5, Unit::GramsPerSecond), 12.5);
}

TEST(DisplayUnitsTest, MetricIsUnchangedAndZeroEconomyHasNoMilesPerGallon) {
    EXPECT_EQ(displayValue(63.4, Unit::KilometresPerHour, DisplayUnits::Metric), 63.4);
    EXPECT_EQ(displayValue(0.0, Unit::LitresPer100Kilometres, DisplayUnits::Metric), 0.0);
    EXPECT_FALSE(
        displayValue(0.0, Unit::LitresPer100Kilometres, DisplayUnits::UsCustomary).has_value());
    EXPECT_FALSE(
        displayValue(-1.0, Unit::LitresPer100Kilometres, DisplayUnits::UsCustomary).has_value());
}

TEST(DisplayUnitsTest, UnitTexts) {
    EXPECT_EQ(displayUnitText(Unit::KilometresPerHour, DisplayUnits::UsCustomary),
              QStringLiteral("mph"));
    EXPECT_EQ(displayUnitText(Unit::DegreesCelsius, DisplayUnits::UsCustomary),
              QStringLiteral("°F"));
    EXPECT_EQ(displayUnitText(Unit::LitresPer100Kilometres, DisplayUnits::UsCustomary),
              QStringLiteral("mpg"));
    EXPECT_EQ(displayUnitText(Unit::Kilometres, DisplayUnits::UsCustomary), QStringLiteral("mi"));
    EXPECT_EQ(displayUnitText(Unit::RevolutionsPerMinute, DisplayUnits::UsCustomary),
              QStringLiteral("rpm"));
    EXPECT_EQ(displayUnitText(Unit::KilometresPerHour, DisplayUnits::Metric),
              QStringLiteral("km/h"));
    EXPECT_EQ(displayUnitText(Unit::DegreesCelsius, DisplayUnits::Metric), QStringLiteral("degC"));
}

TEST(DisplayUnitsTest, TheChoiceIsCommandLineThenCarFileThenTheFallback) {
    bool valid = false;
    EXPECT_EQ(parseDisplayUnits(QStringLiteral(" US ")), DisplayUnits::UsCustomary);
    EXPECT_EQ(parseDisplayUnits(QStringLiteral("Metric")), DisplayUnits::Metric);
    EXPECT_FALSE(parseDisplayUnits(QStringLiteral("imperial")).has_value());
    EXPECT_EQ(chooseDisplayUnits(
                  QStringLiteral("metric"), QStringLiteral("us"), DisplayUnits::UsCustomary, valid),
              DisplayUnits::Metric);
    EXPECT_TRUE(valid);
    EXPECT_EQ(
        chooseDisplayUnits(QString(), QStringLiteral("metric"), DisplayUnits::UsCustomary, valid),
        DisplayUnits::Metric);
    EXPECT_EQ(chooseDisplayUnits(QString(), QString(), DisplayUnits::UsCustomary, valid),
              DisplayUnits::UsCustomary);
    EXPECT_EQ(chooseDisplayUnits(QString(), QString(), DisplayUnits::Metric, valid),
              DisplayUnits::Metric);
    EXPECT_TRUE(valid);
    EXPECT_EQ(chooseDisplayUnits(
                  QStringLiteral("miles"), QStringLiteral("us"), DisplayUnits::Metric, valid),
              DisplayUnits::UsCustomary);
    EXPECT_FALSE(valid);
}

SignalSample validSample(SignalId signalId, double value) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.value = value;
    sample.status = SignalStatus::Valid;
    sample.timestampMilliseconds = 1000;
    return sample;
}

TEST(DisplayUnitsTest, TilesShowUsValuesWhileTheSampleStaysMetric) {
    const VehicleDataViewModel viewModel(DisplayUnits::UsCustomary);
    EXPECT_TRUE(viewModel.usUnits());
    SignalTileModel* speed = viewModel.vehicleSpeed();
    EXPECT_EQ(speed->unitText(), QStringLiteral("mph"));
    speed->applySample(validSample(SignalId::VehicleSpeed, 100.0));
    EXPECT_EQ(speed->valueText(), QStringLiteral("62"));
    SignalTileModel* coolant = viewModel.tile(SignalId::CoolantTemperature);
    coolant->applySample(validSample(SignalId::CoolantTemperature, 52.0));
    EXPECT_EQ(coolant->valueText(), QStringLiteral("126"));
    SignalTileModel* distance = viewModel.tile(SignalId::TripDistance);
    distance->applySample(validSample(SignalId::TripDistance, 0.8));
    EXPECT_EQ(distance->valueText(), QStringLiteral("0.5"));
    EXPECT_EQ(distance->unitText(), QStringLiteral("mi"));
    SignalTileModel* economy = viewModel.tile(SignalId::TripFuelEconomy);
    economy->applySample(validSample(SignalId::TripFuelEconomy, 28.5));
    EXPECT_EQ(economy->valueText(), QStringLiteral("8.3"));
    economy->applySample(validSample(SignalId::TripFuelEconomy, 0.1));
    EXPECT_EQ(economy->valueText(), QStringLiteral(">999"));
    economy->applySample(validSample(SignalId::TripFuelEconomy, 0.0));
    EXPECT_EQ(economy->valueText(), QStringLiteral("--"));
    const VehicleDataViewModel metric;
    EXPECT_FALSE(metric.usUnits());
    EXPECT_EQ(metric.vehicleSpeed()->unitText(), QStringLiteral("km/h"));
}

} // namespace
// NOLINTEND(misc-include-cleaner)
