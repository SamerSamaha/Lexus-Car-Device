#include "lexus_head_unit/hmi/vehicle_data_view_model.h"
#include "lexus_head_unit/hmi/worker_bridge.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/worker_loop.h"
#include "lexus_head_unit/service_dbus/vehicle_data_client.h"
#include "source_factory.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QWindow>

#include <iostream>
#include <memory>
#include <string>

// Qt declares its macros and enumerations in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::KeyValueConfiguration;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::VehicleDataClient;
using lexus_head_unit::VehicleDataViewModel;
using lexus_head_unit::WorkerBridge;
using lexus_head_unit::WorkerLoop;

const char* const dbusSourceKind = "dbus";

// The source, the service layer and the worker thread inside this process: the v0.1.0 shape,
// kept for desk work and for running without the service (--source elm327 or fake).
class InProcessPipeline {
public:
    InProcessPipeline(const KeyValueConfiguration& configuration,
                      const std::string& sourceKind,
                      VehicleDataViewModel& viewModel)
        : m_monitor(m_store, m_clock),
          m_feeder(m_store),
          m_built(lexus_head_unit::app::buildSource(configuration, sourceKind, m_clock)),
          m_loop(*m_built.source, m_feeder, m_monitor, m_built.minimumCycleMilliseconds) {
        lexus_head_unit::app::applyStalenessConfiguration(configuration, m_store);
        QObject::connect(&m_bridge,
                         &WorkerBridge::sampleArrived,
                         &viewModel,
                         &VehicleDataViewModel::onSample,
                         Qt::QueuedConnection);
        QObject::connect(&m_bridge,
                         &WorkerBridge::connectionChanged,
                         &viewModel,
                         &VehicleDataViewModel::onConnectionChanged,
                         Qt::QueuedConnection);
        m_store.setChangeListener([this](const SignalSample& sample) {
            m_bridge.publishSample(sample);
        });
        m_feeder.setTransitionHook([this](const ConnectionTransition& transition) {
            m_bridge.publishConnectionChange(transition);
        });
        if (m_built.fakeSource != nullptr) {
            m_demo = std::make_unique<lexus_head_unit::app::FakeVehicleDemo>(*m_built.fakeSource);
            m_loop.setPerCycleCallback([this]() {
                m_demo->scriptNextCycle(m_clock.nowMilliseconds());
            });
        }
        std::cerr << "lexus-head-unit: source " << m_built.kind << "\n";
    }

    InProcessPipeline(const InProcessPipeline&) = delete;
    InProcessPipeline& operator=(const InProcessPipeline&) = delete;
    InProcessPipeline(InProcessPipeline&&) = delete;
    InProcessPipeline& operator=(InProcessPipeline&&) = delete;

    ~InProcessPipeline() {
        m_loop.stop();
    }

    void start() {
        m_loop.start();
    }

private:
    SteadyClock m_clock;
    SignalStore m_store;
    StalenessMonitor m_monitor;
    SignalStoreFeeder m_feeder;
    lexus_head_unit::app::BuiltSource m_built;
    WorkerBridge m_bridge;
    std::unique_ptr<lexus_head_unit::app::FakeVehicleDemo> m_demo;
    WorkerLoop m_loop;
};

// The v0.2.0 shape (DN-022): the vehicle-data service owns the vehicle; this app is a client.
std::unique_ptr<VehicleDataClient> startClient(const QString& busAddress,
                                               VehicleDataViewModel& viewModel) {
    const QDBusConnection connection =
        busAddress.isEmpty()
            ? QDBusConnection::sessionBus()
            : QDBusConnection::connectToBus(busAddress, QStringLiteral("lexus-head-unit"));
    auto client = std::make_unique<VehicleDataClient>(connection);
    QObject::connect(client.get(),
                     &VehicleDataClient::sampleArrived,
                     &viewModel,
                     &VehicleDataViewModel::onSample);
    QObject::connect(client.get(),
                     &VehicleDataClient::connectionChanged,
                     &viewModel,
                     &VehicleDataViewModel::onConnectionChanged);
    client->start();
    std::cerr << "lexus-head-unit: source dbus (vehicle-data service)\n";
    return client;
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    QGuiApplication application(argumentCount, argumentValues);
    QGuiApplication::setApplicationName(QStringLiteral("lexus-head-unit"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lexus Head Unit vehicle-data application"));
    parser.addHelpOption();
    const QCommandLineOption configOption(QStringLiteral("config"),
                                          QStringLiteral("configuration file"),
                                          QStringLiteral("path"),
                                          QStringLiteral("deploy/head_unit.conf"));
    const QCommandLineOption sourceOption(
        QStringLiteral("source"),
        QStringLiteral("dbus (read the vehicle-data service), elm327 or fake (in this process)"),
        QStringLiteral("kind"));
    const QCommandLineOption busAddressOption(
        QStringLiteral("bus-address"),
        QStringLiteral("with --source dbus: this bus instead of the session bus"),
        QStringLiteral("address"));
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("show the window full screen"));
    const QCommandLineOption recordOption(
        QStringLiteral("record"),
        QStringLiteral("record the ELM327 bytes to this file (keep it in local_recordings/)"),
        QStringLiteral("path"));
    parser.addOption(configOption);
    parser.addOption(sourceOption);
    parser.addOption(recordOption);
    parser.addOption(busAddressOption);
    parser.addOption(fullscreenOption);
    parser.process(application);

    KeyValueConfiguration configuration;
    const std::string configurationPath = parser.value(configOption).toStdString();
    if (!configuration.loadFromFile(configurationPath)) {
        std::cerr << "lexus-head-unit: configuration file " << configurationPath
                  << " not found; using defaults\n";
    }
    if (parser.isSet(recordOption)) {
        configuration.setValue("record.file", parser.value(recordOption).toStdString());
    }

    VehicleDataViewModel viewModel;
    const std::string sourceKind = parser.value(sourceOption).toStdString();
    std::unique_ptr<VehicleDataClient> client;
    std::unique_ptr<InProcessPipeline> pipeline;
    if (sourceKind == dbusSourceKind) {
        client = startClient(parser.value(busAddressOption), viewModel);
    } else {
        pipeline = std::make_unique<InProcessPipeline>(configuration, sourceKind, viewModel);
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

    if (pipeline) {
        pipeline->start();
    }
    return QGuiApplication::exec();
}
// NOLINTEND(misc-include-cleaner)
