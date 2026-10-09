#include "lexus_head_unit/process_support/quit_on_signals.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/session_log.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/worker_loop.h"
#include "lexus_head_unit/service_dbus/vehicle_data_service.h"
#include "source_factory.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QString>

#include <cstdint>
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
using lexus_head_unit::VehicleDataService;
using lexus_head_unit::WorkerLoop;

constexpr int exitCodeNoBus = 1;
constexpr int exitCodeNameTaken = 2;
constexpr std::int64_t sessionLogIntervalMilliseconds = 10000;

QDBusConnection connectToBus(const QString& kind, const QString& address) {
    if (!address.isEmpty()) {
        return QDBusConnection::connectToBus(address, QStringLiteral("lexus-vehicle-data-service"));
    }
    return kind == QStringLiteral("system") ? QDBusConnection::systemBus()
                                            : QDBusConnection::sessionBus();
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    QCoreApplication::setApplicationName(QStringLiteral("lexus-vehicle-data-service"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Lexus Head Unit vehicle-data service: the one process that reads the "
                       "vehicle, publishing its signals on D-Bus"));
    parser.addHelpOption();
    const QCommandLineOption configOption(QStringLiteral("config"),
                                          QStringLiteral("configuration file"),
                                          QStringLiteral("path"),
                                          QStringLiteral("deploy/head_unit.conf"));
    const QCommandLineOption sourceOption(QStringLiteral("source"),
                                          QStringLiteral("source kind: elm327 or fake"),
                                          QStringLiteral("kind"));
    const QCommandLineOption busOption(QStringLiteral("bus"),
                                       QStringLiteral("session (default) or system"),
                                       QStringLiteral("kind"),
                                       QStringLiteral("session"));
    const QCommandLineOption busAddressOption(
        QStringLiteral("bus-address"),
        QStringLiteral("connect to this bus address instead (tests use a private bus)"),
        QStringLiteral("address"));
    const QCommandLineOption recordOption(
        QStringLiteral("record"),
        QStringLiteral("record the ELM327 bytes to this file (keep it in local_recordings/)"),
        QStringLiteral("path"));
    parser.addOption(configOption);
    parser.addOption(sourceOption);
    parser.addOption(recordOption);
    parser.addOption(busOption);
    const QCommandLineOption sessionLogOption(
        QStringLiteral("session-log"),
        QStringLiteral("write transitions and counters to this CSV (LHU-024; keep it in "
                       "local_recordings/)"),
        QStringLiteral("path"));
    parser.addOption(busAddressOption);
    parser.addOption(sessionLogOption);
    parser.process(application);

    KeyValueConfiguration configuration;
    const std::string configurationPath = parser.value(configOption).toStdString();
    if (!configuration.loadFromFile(configurationPath)) {
        std::cerr << "lexus-vehicle-data-service: configuration file " << configurationPath
                  << " not found; using defaults\n";
    }
    if (parser.isSet(recordOption)) {
        configuration.setValue("record.file", parser.value(recordOption).toStdString());
    }

    // The bus and the name come first: a second service must stop before it opens the adapter.
    VehicleDataService service;
    const QDBusConnection connection =
        connectToBus(parser.value(busOption), parser.value(busAddressOption));
    QString registrationError;
    if (!connection.isConnected()) {
        std::cerr << "lexus-vehicle-data-service: no bus: "
                  << connection.lastError().message().toStdString() << "\n";
        return exitCodeNoBus;
    }
    if (!service.registerOn(connection, registrationError)) {
        std::cerr << "lexus-vehicle-data-service: " << registrationError.toStdString() << "\n";
        return exitCodeNameTaken;
    }

    const SteadyClock clock;
    SignalStore store;
    lexus_head_unit::app::applyStalenessConfiguration(configuration, store);
    StalenessMonitor monitor(store, clock);
    SignalStoreFeeder feeder(store);
    lexus_head_unit::app::BuiltSource built = lexus_head_unit::app::buildSource(
        configuration, parser.value(sourceOption).toStdString(), clock);
    store.setChangeListener([&service](const SignalSample& sample) {
        service.publishSample(sample);
    });
    // The session log is written only from the worker thread, where the source and its
    // counters live: the transition hook and the per-cycle callback both run there.
    std::unique_ptr<lexus_head_unit::SessionLog> sessionLog;
    if (parser.isSet(sessionLogOption)) {
        sessionLog = std::make_unique<lexus_head_unit::SessionLog>(
            parser.value(sessionLogOption).toStdString(), sessionLogIntervalMilliseconds);
        if (!sessionLog->isOpen()) {
            std::cerr << "lexus-vehicle-data-service: cannot write the session log\n";
        }
    }
    feeder.setTransitionHook([&service, &sessionLog](const ConnectionTransition& transition) {
        service.publishTransition(transition);
        if (sessionLog) {
            sessionLog->recordTransition(transition);
        }
    });

    std::unique_ptr<lexus_head_unit::app::FakeVehicleDemo> demo;
    WorkerLoop loop(*built.source, feeder, monitor, built.minimumCycleMilliseconds);
    if (built.fakeSource != nullptr) {
        demo = std::make_unique<lexus_head_unit::app::FakeVehicleDemo>(*built.fakeSource);
    }
    lexus_head_unit::VehicleDataSource& source = *built.source;
    loop.setPerCycleCallback([&demo, &clock, &sessionLog, &source]() {
        if (demo) {
            demo->scriptNextCycle(clock.nowMilliseconds());
        }
        if (sessionLog) {
            sessionLog->recordCountersIfDue(
                clock.nowMilliseconds(), source.connectionState(), source.counters());
        }
    });
    if (!lexus_head_unit::installQuitOnSignals()) {
        std::cerr << "lexus-vehicle-data-service: could not install the signal handlers\n";
    }
    std::cerr << "lexus-vehicle-data-service: source " << built.kind << ", bus name owned\n";
    loop.start();
    const int exitCode = QCoreApplication::exec();
    loop.stop();
    return exitCode;
}
// NOLINTEND(misc-include-cleaner)
