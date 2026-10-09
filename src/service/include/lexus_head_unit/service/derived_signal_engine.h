#pragma once

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

constexpr std::size_t rpmBandCount = 4;

// The stated constants of DN-031, section 2. None of them is measured on the car.
struct DerivedSignalConstants {
    // Stoichiometric petrol; E10 would be about 14.1.
    double airFuelRatio = 14.7;
    // Petrol at 15 degrees Celsius; the range is about 720 to 775.
    double fuelDensityGramsPerLitre = 745.0;
    double minimumSpeedForEconomyKilometresPerHour = 5.0;
    std::int64_t speedAgeLimitMilliseconds = 2000;
    double minimumDistanceForAverageKilometres = 0.1;
    std::int64_t integrationGapLimitMilliseconds = 5000;
    std::int64_t tripBreakMilliseconds = 600000;
    double warmCoolantDegreesCelsius = 80.0;
    // Upper bounds of the first three bands; the fourth has none.
    std::array<double, rpmBandCount - 1> rpmBandUpperBounds = {1000.0, 2500.0, 4000.0};
};

// Fuel volume flow from air mass flow: grams of air per second, divided by the air-fuel ratio
// and the fuel density, in litres per hour.
double fuelLitresPerHour(double massAirFlowGramsPerSecond, const DerivedSignalConstants& constants);

// Litres per 100 km at the given air flow and speed. The caller checks the speed is not zero.
double instantFuelEconomy(double massAirFlowGramsPerSecond,
                          double speedKilometresPerHour,
                          const DerivedSignalConstants& constants);

// Trip analytics from live samples (DN-031, REQ-022). Only sample timestamps are used, so a
// replayed session gives the same totals every time. Not thread-safe: the feeder calls it on the
// worker thread.
class DerivedSignalEngine {
public:
    explicit DerivedSignalEngine(DerivedSignalConstants constants = DerivedSignalConstants());

    // A measured, Valid sample in; 0 to 4 derived samples out, each carrying the timestamp of the
    // input. Derived and non-Valid samples give nothing.
    std::vector<SignalSample> onSample(const SignalSample& sample);

    [[nodiscard]] const DerivedSignalConstants& constants() const;
    [[nodiscard]] double tripDistanceKilometres() const;
    [[nodiscard]] double tripFuelLitres() const;
    [[nodiscard]] std::int64_t rpmBandMilliseconds(std::size_t band) const;
    [[nodiscard]] std::optional<std::int64_t> warmUpMilliseconds() const;
    [[nodiscard]] std::uint64_t gapsNotIntegrated() const;
    [[nodiscard]] std::uint64_t tripsStarted() const;

private:
    struct LastInput {
        bool present = false;
        double value = 0.0;
        std::int64_t timestampMilliseconds = 0;
    };

    void startTripIfNeeded(std::int64_t timestampMilliseconds);
    std::optional<std::int64_t> integrationInterval(const LastInput& last,
                                                    std::int64_t timestampMilliseconds);
    [[nodiscard]] std::size_t bandOf(double engineSpeedRpm) const;

    std::vector<SignalSample> onSpeed(const SignalSample& sample);
    std::vector<SignalSample> onEngineSpeed(const SignalSample& sample);
    std::vector<SignalSample> onCoolant(const SignalSample& sample);
    std::vector<SignalSample> onMassAirFlow(const SignalSample& sample);

    DerivedSignalConstants m_constants;
    bool m_tripActive = false;
    std::int64_t m_lastInputMilliseconds = 0;
    LastInput m_speed;
    LastInput m_engineSpeed;
    LastInput m_massAirFlow;
    double m_distanceKilometres = 0.0;
    double m_fuelLitres = 0.0;
    std::array<std::int64_t, rpmBandCount> m_bandMilliseconds{};
    std::optional<std::int64_t> m_warmUpStartMilliseconds;
    std::optional<std::int64_t> m_warmUpMilliseconds;
    std::uint64_t m_gapsNotIntegrated = 0;
    std::uint64_t m_tripsStarted = 0;
};

} // namespace lexus_head_unit
