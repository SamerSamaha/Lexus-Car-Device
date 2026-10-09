#include "lexus_head_unit/hmi/vehicle_data_view_model.h"
#include "lexus_head_unit/hmi/worker_bridge.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/worker_loop.h"
#include "source_factory.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QWindow>

#include <cctype>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

// Qt declares its macros and enumerations in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::allSignalIds;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::definitionOf;
using lexus_head_unit::KeyValueConfiguration;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::VehicleDataViewModel;
using lexus_head_unit::WorkerBridge;
using lexus_head_unit::WorkerLoop;

// Per-signal staleness overrides: staleness.<snake_case_name>_ms, else staleness.default_ms.
void applyStalenessConfiguration(const KeyValueConfiguration& configuration, SignalStore& store) {
    const std::int64_t defaultTimeout = configuration.integerValue(
        "staleness.default_ms", lexus_head_unit::defaultStalenessTimeoutMilliseconds);
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

} // namespace

int main(int argumentCount, char* argumentValues[]) {
    QGuiApplication application(argumentCount, argumentValues);
    QGuiApplication::setApplicationName(QStringLiteral("lexus-head-unit"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lexus Head Unit vehicle-data application"));
    parser.addHelpOption();
    const QCommandLineOption configOption(QStringLiteral("config"),
                                          QStringLiteral("configuration file"),
                                          QStringLiteral("path"),
                                          QStringLiteral("deploy/head_unit.conf"));
    const QCommandLineOption sourceOption(QStringLiteral("source"),
                                          QStringLiteral("source kind: elm327 or fake"),
                                          QStringLiteral("kind"));
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("show the window full screen"));
    parser.addOption(configOption);
    parser.addOption(sourceOption);
    parser.addOption(fullscreenOption);
    parser.process(application);

    KeyValueConfiguration configuration;
    const std::string configurationPath = parser.value(configOption).toStdString();
    if (!configuration.loadFromFile(configurationPath)) {
        std::cerr << "lexus-head-unit: configuration file " << configurationPath
                  << " not found; using defaults\n";
    }

    const SteadyClock clock;
    SignalStore store;
    applyStalenessConfiguration(configuration, store);
    StalenessMonitor monitor(store, clock);
    SignalStoreFeeder feeder(store);
    lexus_head_unit::app::BuiltSource built = lexus_head_unit::app::buildSource(
        configuration, parser.value(sourceOption).toStdString(), clock);
    std::cerr << "lexus-head-unit: source " << built.kind << "\n";

    WorkerBridge bridge;
    VehicleDataViewModel viewModel;
    QObject::connect(&bridge,
                     &WorkerBridge::sampleArrived,
                     &viewModel,
                     &VehicleDataViewModel::onSample,
                     Qt::QueuedConnection);
    QObject::connect(&bridge,
                     &WorkerBridge::connectionChanged,
                     &viewModel,
                     &VehicleDataViewModel::onConnectionChanged,
                     Qt::QueuedConnection);
    store.setChangeListener([&bridge](const SignalSample& sample) {
        bridge.publishSample(sample);
    });
    feeder.setTransitionHook([&bridge](const ConnectionTransition& transition) {
        bridge.publishConnectionChange(transition);
    });

    std::unique_ptr<lexus_head_unit::app::FakeVehicleDemo> demo;
    WorkerLoop loop(*built.source, feeder, monitor, built.minimumCycleMilliseconds);
    if (built.fakeSource != nullptr) {
        demo = std::make_unique<lexus_head_unit::app::FakeVehicleDemo>(*built.fakeSource);
        loop.setPerCycleCallback([&demo, &clock]() {
            demo->scriptNextCycle(clock.nowMilliseconds());
        });
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("vehicleDataContext"), &viewModel);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        []() {
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("LexusHeadUnit"), QStringLiteral("Main"));
    if (parser.isSet(fullscreenOption) && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QWindow*>(engine.rootObjects().first());
        if (window != nullptr) {
            window->setVisibility(QWindow::FullScreen);
        }
    }

    loop.start();
    const int exitCode = QGuiApplication::exec();
    loop.stop();
    return exitCode;
}
// NOLINTEND(misc-include-cleaner)
