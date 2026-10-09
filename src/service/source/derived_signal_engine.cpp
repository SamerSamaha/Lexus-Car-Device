#include "lexus_head_unit/service/derived_signal_engine.h"

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr double secondsPerHour = 3600.0;
constexpr double millisecondsPerHour = 3600000.0;
constexpr double millisecondsPerMinute = 60000.0;
constexpr double kilometresPerHundred = 100.0;

constexpr std::array<SignalId, rpmBandCount> bandSignals = {
    SignalId::TimeBelow1000Rpm,
    SignalId::Time1000To2499Rpm,
    SignalId::Time2500To3999Rpm,
    SignalId::TimeFrom4000Rpm,
};

SignalSample
derivedSample(SignalId signalId, double value, Unit unit, std::int64_t timestampMilliseconds) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.value = value;
    sample.unit = unit;
    sample.timestampMilliseconds = timestampMilliseconds;
    sample.status = SignalStatus::Valid;
    return sample;
}

double minutesOf(std::int64_t milliseconds) {
    return static_cast<double>(milliseconds) / millisecondsPerMinute;
}

} // namespace

double fuelLitresPerHour(double massAirFlowGramsPerSecond,
                         const DerivedSignalConstants& constants) {
    const double fuelGramsPerSecond = massAirFlowGramsPerSecond / constants.airFuelRatio;
    return fuelGramsPerSecond * secondsPerHour / constants.fuelDensityGramsPerLitre;
}

double instantFuelEconomy(double massAirFlowGramsPerSecond,
                          double speedKilometresPerHour,
                          const DerivedSignalConstants& constants) {
    return fuelLitresPerHour(massAirFlowGramsPerSecond, constants) / speedKilometresPerHour *
           kilometresPerHundred;
}

DerivedSignalEngine::DerivedSignalEngine(DerivedSignalConstants constants)
    : m_constants(constants) {}

std::vector<SignalSample> DerivedSignalEngine::onSample(const SignalSample& sample) {
    if (sample.status != SignalStatus::Valid) {
        return {};
    }
    switch (sample.signalId) {
    case SignalId::VehicleSpeed:
        startTripIfNeeded(sample.timestampMilliseconds);
        return onSpeed(sample);
    case SignalId::EngineRpm:
        startTripIfNeeded(sample.timestampMilliseconds);
        return onEngineSpeed(sample);
    case SignalId::CoolantTemperature:
        startTripIfNeeded(sample.timestampMilliseconds);
        return onCoolant(sample);
    case SignalId::MassAirFlow:
        startTripIfNeeded(sample.timestampMilliseconds);
        return onMassAirFlow(sample);
    default:
        return {};
    }
}

void DerivedSignalEngine::startTripIfNeeded(std::int64_t timestampMilliseconds) {
    const bool silentTooLong = m_tripActive && timestampMilliseconds - m_lastInputMilliseconds >
                                                   m_constants.tripBreakMilliseconds;
    if (!m_tripActive || silentTooLong) {
        m_tripActive = true;
        m_speed = LastInput();
        m_engineSpeed = LastInput();
        m_massAirFlow = LastInput();
        m_distanceKilometres = 0.0;
        m_fuelLitres = 0.0;
        m_bandMilliseconds.fill(0);
        m_warmUpStartMilliseconds.reset();
        m_warmUpMilliseconds.reset();
        ++m_tripsStarted;
        m_lastInputMilliseconds = timestampMilliseconds;
    }
    m_lastInputMilliseconds = std::max(m_lastInputMilliseconds, timestampMilliseconds);
}

std::optional<std::int64_t>
DerivedSignalEngine::integrationInterval(const LastInput& last,
                                         std::int64_t timestampMilliseconds) {
    if (!last.present) {
        return std::nullopt;
    }
    const std::int64_t interval = timestampMilliseconds - last.timestampMilliseconds;
    if (interval <= 0) {
        return std::nullopt;
    }
    if (interval > m_constants.integrationGapLimitMilliseconds) {
        ++m_gapsNotIntegrated;
        return std::nullopt;
    }
    return interval;
}

std::size_t DerivedSignalEngine::bandOf(double engineSpeedRpm) const {
    std::size_t band = 0;
    while (band < m_constants.rpmBandUpperBounds.size() &&
           engineSpeedRpm >= m_constants.rpmBandUpperBounds.at(band)) {
        ++band;
    }
    return band;
}

std::vector<SignalSample> DerivedSignalEngine::onSpeed(const SignalSample& sample) {
    const std::optional<std::int64_t> interval =
        integrationInterval(m_speed, sample.timestampMilliseconds);
    if (interval.has_value()) {
        const double averageSpeed = (m_speed.value + sample.value) / 2.0;
        m_distanceKilometres += averageSpeed * static_cast<double>(*interval) / millisecondsPerHour;
    }
    m_speed = LastInput{true, sample.value, sample.timestampMilliseconds};
    return {derivedSample(SignalId::TripDistance,
                          m_distanceKilometres,
                          Unit::Kilometres,
                          sample.timestampMilliseconds)};
}

std::vector<SignalSample> DerivedSignalEngine::onEngineSpeed(const SignalSample& sample) {
    const std::optional<std::int64_t> interval =
        integrationInterval(m_engineSpeed, sample.timestampMilliseconds);
    if (interval.has_value()) {
        m_bandMilliseconds.at(bandOf(m_engineSpeed.value)) += *interval;
    }
    m_engineSpeed = LastInput{true, sample.value, sample.timestampMilliseconds};
    std::vector<SignalSample> samples;
    samples.reserve(rpmBandCount);
    for (std::size_t band = 0; band < rpmBandCount; ++band) {
        samples.push_back(derivedSample(bandSignals.at(band),
                                        minutesOf(m_bandMilliseconds.at(band)),
                                        Unit::Minutes,
                                        sample.timestampMilliseconds));
    }
    return samples;
}

std::vector<SignalSample> DerivedSignalEngine::onCoolant(const SignalSample& sample) {
    if (!m_warmUpStartMilliseconds.has_value()) {
        m_warmUpStartMilliseconds = sample.timestampMilliseconds;
    }
    if (!m_warmUpMilliseconds.has_value() &&
        sample.value >= m_constants.warmCoolantDegreesCelsius) {
        m_warmUpMilliseconds = sample.timestampMilliseconds - m_warmUpStartMilliseconds.value_or(0);
    }
    if (!m_warmUpMilliseconds.has_value()) {
        return {};
    }
    return {derivedSample(SignalId::CoolantWarmUpTime,
                          minutesOf(m_warmUpMilliseconds.value_or(0)),
                          Unit::Minutes,
                          sample.timestampMilliseconds)};
}

std::vector<SignalSample> DerivedSignalEngine::onMassAirFlow(const SignalSample& sample) {
    const std::optional<std::int64_t> interval =
        integrationInterval(m_massAirFlow, sample.timestampMilliseconds);
    if (interval.has_value()) {
        const double averageLitresPerHour = (fuelLitresPerHour(m_massAirFlow.value, m_constants) +
                                             fuelLitresPerHour(sample.value, m_constants)) /
                                            2.0;
        m_fuelLitres += averageLitresPerHour * static_cast<double>(*interval) / millisecondsPerHour;
    }
    m_massAirFlow = LastInput{true, sample.value, sample.timestampMilliseconds};

    std::vector<SignalSample> samples;
    const bool speedRecent = m_speed.present && std::llabs(sample.timestampMilliseconds -
                                                           m_speed.timestampMilliseconds) <=
                                                    m_constants.speedAgeLimitMilliseconds;
    if (speedRecent && m_speed.value >= m_constants.minimumSpeedForEconomyKilometresPerHour) {
        samples.push_back(
            derivedSample(SignalId::InstantFuelEconomy,
                          instantFuelEconomy(sample.value, m_speed.value, m_constants),
                          Unit::LitresPer100Kilometres,
                          sample.timestampMilliseconds));
    }
    if (m_distanceKilometres >= m_constants.minimumDistanceForAverageKilometres) {
        samples.push_back(derivedSample(SignalId::TripFuelEconomy,
                                        m_fuelLitres / m_distanceKilometres * kilometresPerHundred,
                                        Unit::LitresPer100Kilometres,
                                        sample.timestampMilliseconds));
    }
    return samples;
}

const DerivedSignalConstants& DerivedSignalEngine::constants() const {
    return m_constants;
}

double DerivedSignalEngine::tripDistanceKilometres() const {
    return m_distanceKilometres;
}

double DerivedSignalEngine::tripFuelLitres() const {
    return m_fuelLitres;
}

std::int64_t DerivedSignalEngine::rpmBandMilliseconds(std::size_t band) const {
    return m_bandMilliseconds.at(band);
}

std::optional<std::int64_t> DerivedSignalEngine::warmUpMilliseconds() const {
    return m_warmUpMilliseconds;
}

std::uint64_t DerivedSignalEngine::gapsNotIntegrated() const {
    return m_gapsNotIntegrated;
}

std::uint64_t DerivedSignalEngine::tripsStarted() const {
    return m_tripsStarted;
}

} // namespace lexus_head_unit
