#include "lexus_head_unit/hmi/connection_status_model.h"
#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/app_registry.h"
#include "lexus_head_unit/hub/command_line.h"
#include "lexus_head_unit/hub/hub_control_server.h"
#include "lexus_head_unit/hub/hub_view_model.h"
#include "lexus_head_unit/hub/posix_process_launcher.h"
#include "lexus_head_unit/hub/power_status_model.h"
#include "lexus_head_unit/process_support/quit_on_signals.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service_dbus/vehicle_data_client.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QStringList>
#include <QWindow>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// Qt declares its macros and enumerations in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::AppManagerSettings;
using lexus_head_unit::AppProcessManager;
using lexus_head_unit::AppRegistry;
using lexus_head_unit::HubControlServer;
using lexus_head_unit::HubViewModel;
using lexus_head_unit::KeyValueConfiguration;
using lexus_head_unit::PosixProcessLauncher;
using lexus_head_unit::SteadyClock;

constexpr int pollIntervalMilliseconds = 100;
constexpr std::int64_t defaultPowerPollMilliseconds = 2000;
constexpr int sendTimeoutMilliseconds = 3000;
constexpr std::int64_t shutdownWaitMilliseconds = 3000;
constexpr int exitCodeCommandFailed = 1;
constexpr int exitCodeAlreadyRunning = 2;

const char* const defaultSocketName = "lexus-hub";

// "lexus-hub --send <command> [--control-socket <name>]" is a client, not a hub: it must not
// open a window, so it is recognised before any GUI object exists.
bool isClientInvocation(int argumentCount, char** argumentValues) {
    for (int index = 1; index < argumentCount; ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (std::strcmp(argumentValues[index], "--send") == 0) {
            return true;
        }
    }
    return false;
}

// A command line from the registry file, split without a shell; the default if the key is
// missing or the text cannot be split.
QStringList commandFrom(const KeyValueConfiguration& configuration,
                        const std::string& key,
                        const std::string& defaultCommand) {
    const std::optional<std::vector<std::string>> words =
        lexus_head_unit::splitCommandLine(configuration.stringValue(key, defaultCommand));
    QStringList command;
    for (const std::string& word : words.value_or(std::vector<std::string>{})) {
        command << QString::fromStdString(word);
    }
    return command;
}

QCommandLineOption socketOption() {
    return {QStringLiteral("control-socket"),
            QStringLiteral("name of the hub's local control socket"),
            QStringLiteral("name"),
            QString::fromLatin1(defaultSocketName)};
}

int runClient(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    QCommandLineParser parser;
    const QCommandLineOption sendOption(QStringLiteral("send"),
                                        QStringLiteral("command for a running hub"),
                                        QStringLiteral("command"));
    const QCommandLineOption socket = socketOption();
    parser.addOption(sendOption);
    parser.addOption(socket);
    parser.process(application);
    bool answered = false;
    const QString reply = lexus_head_unit::sendHubCommand(
        parser.value(socket), parser.value(sendOption), sendTimeoutMilliseconds, answered);
    std::cout << reply.toStdString() << "\n";
    return answered ? 0 : exitCodeCommandFailed;
}

int runHub(int argumentCount, char** argumentValues) {
    QGuiApplication application(argumentCount, argumentValues);
    QGuiApplication::setApplicationName(QStringLiteral("lexus-hub"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lexus Head Unit app hub"));
    parser.addHelpOption();
    const QCommandLineOption registryOption(QStringLiteral("registry"),
                                            QStringLiteral("app registry file"),
                                            QStringLiteral("path"),
                                            QStringLiteral("deploy/hub.conf"));
    const QCommandLineOption socket = socketOption();
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("show the window full screen"));
    parser.addOption(registryOption);
    parser.addOption(socket);
    parser.addOption(fullscreenOption);
    parser.process(application);

    KeyValueConfiguration configuration;
    const std::string registryPath = parser.value(registryOption).toStdString();
    QStringList errors;
    if (!configuration.loadFromFile(registryPath)) {
        errors << QStringLiteral("registry file %1 not found").arg(parser.value(registryOption));
    }
    const AppRegistry registry = AppRegistry::fromConfiguration(configuration);
    if (errors.isEmpty()) {
        for (const std::string& error : registry.errors()) {
            errors << QString::fromStdString(error);
        }
    }
    for (const QString& error : errors) {
        std::cerr << "lexus-hub: " << error.toStdString() << "\n";
    }

    AppManagerSettings settings;
    settings.stopGraceMilliseconds =
        configuration.integerValue("hub.stop_grace_ms", settings.stopGraceMilliseconds);
    const SteadyClock clock;
    PosixProcessLauncher launcher;
    AppProcessManager manager(launcher, clock, settings);
    HubViewModel hub(manager, registry.entries(), errors);
    HubControlServer controlServer(hub);
    QString listenError;
    if (!controlServer.listen(parser.value(socket), listenError)) {
        std::cerr << "lexus-hub: " << listenError.toStdString() << "\n";
        return exitCodeAlreadyRunning;
    }
    if (!lexus_head_unit::installQuitOnSignals()) {
        std::cerr << "lexus-hub: could not install the signal handlers\n";
    }

    // The status strip (DN-025): connection state from the vehicle-data service over D-Bus,
    // the firmware power flags, and the two-tap shutdown.
    lexus_head_unit::ConnectionStatusModel connection;
    lexus_head_unit::VehicleDataClient vehicleData(QDBusConnection::sessionBus());
    QObject::connect(&vehicleData,
                     &lexus_head_unit::VehicleDataClient::connectionChanged,
                     &connection,
                     &lexus_head_unit::ConnectionStatusModel::applyTransition);
    vehicleData.start();
    const QStringList powerCommand =
        commandFrom(configuration, "hub.power_command", "vcgencmd get_throttled");
    lexus_head_unit::ProcessPowerStatusReader powerReader(powerCommand.value(0),
                                                          powerCommand.mid(1));
    lexus_head_unit::PowerStatusModel power(powerReader);
    lexus_head_unit::ShutdownController shutdown(
        commandFrom(configuration, "hub.shutdown_command", "systemctl poweroff"),
        lexus_head_unit::ShutdownController::detachedProcessExecutor());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hubContext"), &hub);
    engine.rootContext()->setContextProperty(QStringLiteral("connectionContext"), &connection);
    engine.rootContext()->setContextProperty(QStringLiteral("powerContext"), &power);
    engine.rootContext()->setContextProperty(QStringLiteral("shutdownContext"), &shutdown);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        []() {
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("LexusHub"), QStringLiteral("Main"));
    QWindow* window = engine.rootObjects().isEmpty()
                          ? nullptr
                          : qobject_cast<QWindow*>(engine.rootObjects().first());
    if (window != nullptr && parser.isSet(fullscreenOption)) {
        window->setVisibility(QWindow::FullScreen);
    }
    controlServer.setWindowVisibilityProvider([window]() {
        return window != nullptr && window->isVisible();
    });

    hub.startPolling(pollIntervalMilliseconds);
    power.startPolling(static_cast<int>(
        configuration.integerValue("hub.power_poll_ms", defaultPowerPollMilliseconds)));
    std::cerr << "lexus-hub: " << registry.entries().size() << " apps, control socket "
              << parser.value(socket).toStdString() << "\n";
    const int exitCode = QGuiApplication::exec();
    manager.shutdown(shutdownWaitMilliseconds);
    return exitCode;
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    if (isClientInvocation(argumentCount, argumentValues)) {
        return runClient(argumentCount, argumentValues);
    }
    return runHub(argumentCount, argumentValues);
}
// NOLINTEND(misc-include-cleaner)
