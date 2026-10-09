// Verifies: REQ-004, REQ-010

#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/service/signal_id.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::DecodedPid;
using lexus_head_unit::decodePid;
using lexus_head_unit::expectedDataByteCount;
using lexus_head_unit::ObdPid;
using lexus_head_unit::pidByte;
using lexus_head_unit::pidForSignal;
using lexus_head_unit::signalForPid;
using lexus_head_unit::SignalId;
using lexus_head_unit::Unit;

struct Vector {
    ObdPid pid;
    std::vector<std::uint8_t> dataBytes;
    double expectedValue;
    Unit expectedUnit;
};

// Five vectors per PID: minimum raw, maximum raw and three mid-range values, computed by hand
// from the SAE J1979 formulas.
const std::vector<Vector>& vectors() {
    static const std::vector<Vector> table = {
        {ObdPid::VehicleSpeed, {0x00}, 0.0, Unit::KilometresPerHour},
        {ObdPid::VehicleSpeed, {0xFF}, 255.0, Unit::KilometresPerHour},
        {ObdPid::VehicleSpeed, {0x3C}, 60.0, Unit::KilometresPerHour},
        {ObdPid::VehicleSpeed, {0x64}, 100.0, Unit::KilometresPerHour},
        {ObdPid::VehicleSpeed, {0x01}, 1.0, Unit::KilometresPerHour},

        {ObdPid::EngineRpm, {0x00, 0x00}, 0.0, Unit::RevolutionsPerMinute},
        {ObdPid::EngineRpm, {0xFF, 0xFF}, 16383.75, Unit::RevolutionsPerMinute},
        {ObdPid::EngineRpm, {0x0C, 0x80}, 800.0, Unit::RevolutionsPerMinute},
        {ObdPid::EngineRpm, {0x1A, 0xF8}, 1726.0, Unit::RevolutionsPerMinute},
        {ObdPid::EngineRpm, {0x00, 0x01}, 0.25, Unit::RevolutionsPerMinute},

        {ObdPid::CoolantTemperature, {0x00}, -40.0, Unit::DegreesCelsius},
        {ObdPid::CoolantTemperature, {0xFF}, 215.0, Unit::DegreesCelsius},
        {ObdPid::CoolantTemperature, {0x28}, 0.0, Unit::DegreesCelsius},
        {ObdPid::CoolantTemperature, {0x7B}, 83.0, Unit::DegreesCelsius},
        {ObdPid::CoolantTemperature, {0x82}, 90.0, Unit::DegreesCelsius},

        {ObdPid::EngineLoad, {0x00}, 0.0, Unit::Percent},
        {ObdPid::EngineLoad, {0xFF}, 100.0, Unit::Percent},
        {ObdPid::EngineLoad, {0x33}, 20.0, Unit::Percent},
        {ObdPid::EngineLoad, {0x80}, 50.19607843137255, Unit::Percent},
        {ObdPid::EngineLoad, {0x01}, 0.39215686274509803, Unit::Percent},

        {ObdPid::ThrottlePosition, {0x00}, 0.0, Unit::Percent},
        {ObdPid::ThrottlePosition, {0xFF}, 100.0, Unit::Percent},
        {ObdPid::ThrottlePosition, {0x19}, 9.803921568627452, Unit::Percent},
        {ObdPid::ThrottlePosition, {0x40}, 25.098039215686274, Unit::Percent},
        {ObdPid::ThrottlePosition, {0xCC}, 80.0, Unit::Percent},

        {ObdPid::IntakeAirTemperature, {0x00}, -40.0, Unit::DegreesCelsius},
        {ObdPid::IntakeAirTemperature, {0xFF}, 215.0, Unit::DegreesCelsius},
        {ObdPid::IntakeAirTemperature, {0x41}, 25.0, Unit::DegreesCelsius},
        {ObdPid::IntakeAirTemperature, {0x1E}, -10.0, Unit::DegreesCelsius},
        {ObdPid::IntakeAirTemperature, {0x50}, 40.0, Unit::DegreesCelsius},

        {ObdPid::ControlModuleVoltage, {0x00, 0x00}, 0.0, Unit::Volts},
        {ObdPid::ControlModuleVoltage, {0xFF, 0xFF}, 65.535, Unit::Volts},
        {ObdPid::ControlModuleVoltage, {0x37, 0x14}, 14.1, Unit::Volts},
        {ObdPid::ControlModuleVoltage, {0x2E, 0xE0}, 12.0, Unit::Volts},
        {ObdPid::ControlModuleVoltage, {0x00, 0x64}, 0.1, Unit::Volts},

        {ObdPid::FuelLevel, {0x00}, 0.0, Unit::Percent},
        {ObdPid::FuelLevel, {0xFF}, 100.0, Unit::Percent},
        {ObdPid::FuelLevel, {0xA0}, 62.745098039215684, Unit::Percent},
        {ObdPid::FuelLevel, {0x66}, 40.0, Unit::Percent},
        {ObdPid::FuelLevel, {0x0A}, 3.9215686274509802, Unit::Percent},
    };
    return table;
}

std::string describe(const Vector& vector) {
    std::string text = "pid 0x" + std::to_string(pidByte(vector.pid)) + " bytes";
    for (const std::uint8_t byte : vector.dataBytes) {
        text += " " + std::to_string(byte);
    }
    return text;
}

// Returns true when the vector decodes to the expected value, unit and signal.
bool matchesFormula(const Vector& vector) {
    const std::optional<DecodedPid> decoded = decodePid(pidByte(vector.pid), vector.dataBytes);
    if (!decoded.has_value()) {
        ADD_FAILURE() << describe(vector) << ": no value";
        return false;
    }
    EXPECT_NEAR(decoded->value, vector.expectedValue, 1e-9) << describe(vector);
    EXPECT_EQ(decoded->unit, vector.expectedUnit) << describe(vector);
    EXPECT_EQ(decoded->signalId, signalForPid(pidByte(vector.pid)).value_or(SignalId::FuelLevel))
        << describe(vector);
    EXPECT_TRUE(std::isfinite(decoded->value)) << describe(vector);
    return std::abs(decoded->value - vector.expectedValue) <= 1e-9 &&
           decoded->unit == vector.expectedUnit;
}

TEST(ObdPidDecoderTest, FortyHandComputedVectorsMatchTheSaeFormulas) {
    ASSERT_EQ(vectors().size(), 40U);
    std::size_t matched = 0;
    for (const Vector& vector : vectors()) {
        if (matchesFormula(vector)) {
            ++matched;
        }
    }
    EXPECT_EQ(matched, 40U);
}

// True when the signal has a PID and that PID maps back to the same signal.
bool mapsBackToItself(SignalId signalId) {
    const std::optional<ObdPid> pid = pidForSignal(signalId);
    if (!pid.has_value()) {
        return false;
    }
    return signalForPid(pidByte(*pid)) == signalId;
}

TEST(ObdPidDecoderTest, PidAndSignalMappingsAreInverseOnTheMeasuredSignals) {
    std::size_t inverse = 0;
    for (const SignalId signalId : lexus_head_unit::measuredSignalIds) {
        inverse += mapsBackToItself(signalId) ? 1U : 0U;
    }
    EXPECT_EQ(inverse, lexus_head_unit::measuredSignalCount);
}

TEST(ObdPidDecoderTest, DerivedSignalsHaveNoPidAndUnknownPidsHaveNoSignal) {
    std::size_t withPid = 0;
    for (const SignalId signalId : lexus_head_unit::derivedSignalIds) {
        withPid += pidForSignal(signalId).has_value() ? 1U : 0U;
    }
    EXPECT_EQ(withPid, 0U);
    EXPECT_FALSE(signalForPid(0x00).has_value());
    EXPECT_EQ(signalForPid(0x10), SignalId::MassAirFlow);
    EXPECT_FALSE(signalForPid(0x5E).has_value());
    EXPECT_FALSE(signalForPid(0xFF).has_value());
}

TEST(ObdPidDecoderTest, MassAirFlowIsTwoBytesInHundredthsOfAGramPerSecond) {
    // 0x03E8 = 1000, so 10.00 g/s; 0xFFFF is the top of the range, 655.35 g/s.
    const DecodedPid decoded = decodePid(0x10, {0x03, 0xE8}).value_or(DecodedPid{});
    EXPECT_EQ(decoded.signalId, SignalId::MassAirFlow);
    EXPECT_DOUBLE_EQ(decoded.value, 10.0);
    EXPECT_EQ(decoded.unit, Unit::GramsPerSecond);
    EXPECT_DOUBLE_EQ(decodePid(0x10, {0xFF, 0xFF}).value_or(DecodedPid{}).value, 655.35);
    EXPECT_FALSE(decodePid(0x10, {0x03}).has_value());
}

TEST(ObdPidDecoderTest, ExpectedByteCountsAreKnownForTheEightPidsAndTheBitmaps) {
    EXPECT_EQ(expectedDataByteCount(0x0C).value_or(0), 2U);
    EXPECT_EQ(expectedDataByteCount(0x42).value_or(0), 2U);
    EXPECT_EQ(expectedDataByteCount(0x0D).value_or(0), 1U);
    EXPECT_EQ(expectedDataByteCount(0x00).value_or(0), 4U);
    EXPECT_EQ(expectedDataByteCount(0x20).value_or(0), 4U);
    EXPECT_EQ(expectedDataByteCount(0x40).value_or(0), 4U);
    EXPECT_EQ(expectedDataByteCount(0x10).value_or(0), 2U);
    EXPECT_FALSE(expectedDataByteCount(0x5E).has_value());
    EXPECT_FALSE(expectedDataByteCount(0xA6).has_value());
}

TEST(ObdPidDecoderTest, WrongByteCountOrUnknownPidGivesNoValue) {
    EXPECT_FALSE(decodePid(0x0D, {}).has_value());
    EXPECT_FALSE(decodePid(0x0D, {0x10, 0x20}).has_value());
    EXPECT_FALSE(decodePid(0x0C, {0x10}).has_value());
    EXPECT_FALSE(decodePid(0x0C, {0x10, 0x20, 0x30}).has_value());
    EXPECT_FALSE(decodePid(0x00, {0xBE, 0x3F, 0xA8, 0x13}).has_value());
    EXPECT_FALSE(decodePid(0x5E, {0x01, 0x02}).has_value());
    EXPECT_FALSE(decodePid(0xFF, {0x01}).has_value());
}

TEST(ObdPidDecoderTest, RandomPidAndByteStringsNeverCrashAndOnlyWellFormedPairsDecode) {
    // A fixed seed on purpose: the run is reproducible and a failure can be replayed.
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp)
    std::mt19937 generator(20261008);
    std::uniform_int_distribution<int> byteDistribution(0, 255);
    std::uniform_int_distribution<std::size_t> lengthDistribution(0, 6);
    std::size_t decodedCount = 0;
    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto pid = static_cast<std::uint8_t>(byteDistribution(generator));
        std::vector<std::uint8_t> dataBytes(lengthDistribution(generator));
        for (std::uint8_t& byte : dataBytes) {
            byte = static_cast<std::uint8_t>(byteDistribution(generator));
        }
        const std::optional<DecodedPid> decoded = decodePid(pid, dataBytes);
        const bool wellFormed = signalForPid(pid).has_value() &&
                                expectedDataByteCount(pid).value_or(0) == dataBytes.size();
        EXPECT_EQ(decoded.has_value(), wellFormed);
        if (decoded.has_value()) {
            EXPECT_TRUE(std::isfinite(decoded->value));
            ++decodedCount;
        }
    }
    EXPECT_GT(decodedCount, 0U);
}

} // namespace
