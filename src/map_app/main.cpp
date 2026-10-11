#include "lexus_head_unit/hmi/map_view_model.h"
#include "lexus_head_unit/hub/car_status_models.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service_dbus/vehicle_data_client.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCursor>
#include <QDBusConnection>
#include <QDir>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QString>
#include <QWindow>

#include <iostream>
#include <optional>
#include <string>

// Qt declares its macros and enumerations in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

constexpr int recordingRefreshMilliseconds = 1000;
constexpr double defaultHomeRadiusMetres = 500.0;

// The home area from [privacy] in car.conf (DN-054): home_latitude, home_longitude and
// home_radius_m (default 500). Nothing when either coordinate is missing or not a number.
std::optional<lexus_head_unit::HomeArea>
homeArea(const lexus_head_unit::KeyValueConfiguration& configuration) {
    bool latitudeValid = false;
    bool longitudeValid = false;
    bool radiusValid = false;
    const double latitude =
        QString::fromStdString(configuration.stringValue("privacy.home_latitude", ""))
            .toDouble(&latitudeValid);
    const double longitude =
        QString::fromStdString(configuration.stringValue("privacy.home_longitude", ""))
            .toDouble(&longitudeValid);
    const double radius =
        QString::fromStdString(configuration.stringValue("privacy.home_radius_m", ""))
            .toDouble(&radiusValid);
    if (!latitudeValid || !longitudeValid) {
        return std::nullopt;
    }
    return lexus_head_unit::HomeArea{
        latitude, longitude, radiusValid && radius > 0.0 ? radius : defaultHomeRadiusMetres};
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    QGuiApplication application(argumentCount, argumentValues);
    QGuiApplication::setApplicationName(QStringLiteral("lexus-map"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lexus Head Unit map of the phone's position"));
    parser.addHelpOption();
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("show the window full screen"));
    const QCommandLineOption hidePointerOption(
        QStringLiteral("hide-pointer"),
        QStringLiteral("no mouse pointer over the window (a touch screen in the car)"));
    const QCommandLineOption carConfigOption(
        QStringLiteral("car-config"),
        QStringLiteral("the car-mode file with the [privacy] home area"),
        QStringLiteral("path"),
        QDir::homePath() + QStringLiteral("/.config/lexus-head-unit/car.conf"));
    const QCommandLineOption recordingStatusOption(
        QStringLiteral("recording-status"),
        QStringLiteral("status file of lexus-screen-record: the position near home is hidden "
                       "while it exists (REQ-030)"),
        QStringLiteral("path"));
    const QCommandLineOption busAddressOption(QStringLiteral("bus-address"),
                                              QStringLiteral("this bus instead of the session bus"),
                                              QStringLiteral("address"));
    parser.addOption(fullscreenOption);
    parser.addOption(hidePointerOption);
    parser.addOption(carConfigOption);
    parser.addOption(recordingStatusOption);
    parser.addOption(busAddressOption);
    parser.process(application);
    if (parser.isSet(hidePointerOption)) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
        std::cerr << "lexus-map: pointer hidden\n";
    }

    lexus_head_unit::KeyValueConfiguration carConfiguration;
    carConfiguration.loadFromFile(parser.value(carConfigOption).toStdString());
    const std::optional<lexus_head_unit::HomeArea> home = homeArea(carConfiguration);
    const bool maskAlways =
        carConfiguration.stringValue("privacy.mask_home_always", "off") == std::string("on");
    // Never the coordinates: only whether a home area is set.
    std::cerr << "lexus-map: home area " << (home ? "set" : "not set")
              << (maskAlways ? ", hidden always" : ", hidden while recording") << "\n";

    const QString recordingStatus =
        parser.isSet(recordingStatusOption)
            ? parser.value(recordingStatusOption)
            : qEnvironmentVariable("XDG_RUNTIME_DIR") + QStringLiteral("/lexus-screen-recording");
    lexus_head_unit::RecordingStatusModel recording(recordingStatus);
    recording.startPolling(recordingRefreshMilliseconds);
    lexus_head_unit::MapViewModel map(home, maskAlways, [&recording]() {
        return recording.recording();
    });
    QObject::connect(&recording,
                     &lexus_head_unit::RecordingStatusModel::changed,
                     &map,
                     &lexus_head_unit::MapViewModel::refresh);

    const QDBusConnection connection =
        parser.isSet(busAddressOption)
            ? QDBusConnection::connectToBus(parser.value(busAddressOption),
                                            QStringLiteral("lexus-map"))
            : QDBusConnection::sessionBus();
    lexus_head_unit::VehicleDataClient client(connection);
    QObject::connect(&client,
                     &lexus_head_unit::VehicleDataClient::sampleArrived,
                     &map,
                     &lexus_head_unit::MapViewModel::onSample);
    client.start();

    const QString cacheDirectory =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tiles");
    QDir().mkpath(cacheDirectory);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mapContext"), &map);
    engine.rootContext()->setContextProperty(QStringLiteral("mapCacheDirectoryContext"),
                                             cacheDirectory);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        []() {
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("LexusMap"), QStringLiteral("Main"));
    if (parser.isSet(fullscreenOption) && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QWindow*>(engine.rootObjects().first());
        if (window != nullptr) {
            window->setVisibility(QWindow::FullScreen);
        }
    }
    return QGuiApplication::exec();
}
// NOLINTEND(misc-include-cleaner)
