#include "source_factory.h"

#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/fake_source.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/hardware/socket_can_dbc_source.h"
#include "lexus_head_unit/hardware/socket_can_frame_reader.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_store.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace lexus_head_unit::app {

void applyStalenessConfiguration(const KeyValueConfiguration& configuration, SignalStore& store) {
    const std::int64_t defaultTimeout =
        configuration.integerValue("staleness.default_ms", defaultStalenessTimeoutMilliseconds);
    for (const SignalId signalId : allSignalIds) {
        std::string key = "staleness.";
        for (const char character : definitionOf(signalId).name) {
            const auto lowered =
                static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            key.push_back(character == ' ' ? '_' : lowered);
        }
        key += "_ms";
        store.setStalenessTimeout(signalId, configuration.integerValue(key, defaultTimeout));
    }
}

namespace {

constexpr std::int64_t fakeCycleMilliseconds = 200;
constexpr double millisecondsPerSecond = 1000.0;

// The demo vehicle: a parked car idling, with values that move so the screen can be seen.
namespace demo {
constexpr double speedBaseKmh = 40.0;
constexpr double speedSwingKmh = 30.0;
constexpr double speedPeriodSeconds = 7.0;
constexpr double rpmBase = 800.0;
constexpr double rpmSwing = 400.0;
constexpr double rpmPeriodSeconds = 3.0;
constexpr double coolantStartCelsius = 20.0;
constexpr double coolantMaximumCelsius = 90.0;
constexpr double coolantWarmUpCelsiusPerSecond = 2.0;
constexpr double loadBasePercent = 30.0;
constexpr double loadSwingPercent = 10.0;
constexpr double loadPeriodSeconds = 5.0;
constexpr double throttleBasePercent = 12.0;
constexpr double throttleSwingPercent = 5.0;
constexpr double throttlePeriodSeconds = 4.0;
constexpr double intakeAirCelsius = 25.0;
constexpr double voltageBase = 14.1;
constexpr double voltageSwing = 0.1;
constexpr double fuelStartPercent = 63.0;
constexpr double fuelSecondsPerPercent = 600.0;
} // namespace demo

} // namespace

BuiltSource buildSource(const KeyValueConfiguration& configuration,
                        const std::string& kindOverride,
                        const Clock& clock) {
    BuiltSource built;
    built.kind =
        kindOverride.empty() ? configuration.stringValue("source.kind", "elm327") : kindOverride;
    if (built.kind == "elm327") {
        const Elm327SourceConfiguration elm327Configuration =
            Elm327SourceConfiguration::fromConfiguration(configuration);
        auto transport =
            std::make_unique<FileDescriptorByteTransport>(elm327Configuration.devicePath);
        built.source = std::make_unique<Elm327ObdSource>(*transport, clock, elm327Configuration);
        built.transport = std::move(transport);
        return built;
    }
    if (built.kind == "can") {
        const CanSourceConfiguration canConfiguration =
            CanSourceConfiguration::fromConfiguration(configuration);
        auto reader = std::make_unique<SocketCanFrameReader>(canConfiguration.interfaceName);
        built.source = std::make_unique<SocketCanDbcSource>(
            *reader, DbcDatabase::loadFromFile(canConfiguration.dbcPath), clock, canConfiguration);
        built.canReader = std::move(reader);
        return built;
    }
    built.kind = "fake";
    auto fake = std::make_unique<FakeSource>(clock);
    built.fakeSource = fake.get();
    built.source = std::move(fake);
    built.minimumCycleMilliseconds = fakeCycleMilliseconds;
    return built;
}

FakeVehicleDemo::FakeVehicleDemo(FakeSource& source) : m_source(&source) {}

void FakeVehicleDemo::scriptNextCycle(std::int64_t nowMilliseconds) {
    if (m_startMilliseconds < 0) {
        m_startMilliseconds = nowMilliseconds;
    }
    const double seconds =
        static_cast<double>(nowMilliseconds - m_startMilliseconds) / millisecondsPerSecond;
    if (m_source->pendingSteps() > 0) {
        return;
    }
    m_source->scriptSample(
        SignalId::VehicleSpeed,
        demo::speedBaseKmh + (demo::speedSwingKmh * std::sin(seconds / demo::speedPeriodSeconds)));
    m_source->scriptSample(
        SignalId::EngineRpm,
        demo::rpmBase + (demo::rpmSwing * (1.0 + std::sin(seconds / demo::rpmPeriodSeconds))));
    m_source->scriptSample(
        SignalId::CoolantTemperature,
        std::fmin(demo::coolantMaximumCelsius,
                  demo::coolantStartCelsius + (seconds * demo::coolantWarmUpCelsiusPerSecond)));
    m_source->scriptSample(
        SignalId::EngineLoad,
        demo::loadBasePercent +
            (demo::loadSwingPercent * (1.0 + std::sin(seconds / demo::loadPeriodSeconds))));
    m_source->scriptSample(
        SignalId::ThrottlePosition,
        demo::throttleBasePercent +
            (demo::throttleSwingPercent * (1.0 + std::sin(seconds / demo::throttlePeriodSeconds))));
    m_source->scriptSample(SignalId::IntakeAirTemperature, demo::intakeAirCelsius);
    m_source->scriptSample(SignalId::ControlModuleVoltage,
                           demo::voltageBase + (demo::voltageSwing * std::sin(seconds)));
    m_source->scriptSample(
        SignalId::FuelLevel,
        std::fmax(0.0, demo::fuelStartPercent - (seconds / demo::fuelSecondsPerPercent)));
}

} // namespace lexus_head_unit::app
