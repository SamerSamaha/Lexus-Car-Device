#include "lexus_head_unit/hmi/map_view_model.h"

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>

#include <cmath>
#include <optional>
#include <utility>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr double earthRadiusMetres = 6371000.0;
constexpr double degreesPerHalfTurn = 180.0;
constexpr double halfTurnRadians = 3.14159265358979323846;
constexpr double halves = 2.0;

double radians(double degrees) {
    return degrees * halfTurnRadians / degreesPerHalfTurn;
}

} // namespace

double
distanceMetres(double fromLatitude, double fromLongitude, double toLatitude, double toLongitude) {
    const double latitudeChange = radians(toLatitude - fromLatitude);
    const double longitudeChange = radians(toLongitude - fromLongitude);
    const double halfChord =
        (std::sin(latitudeChange / halves) * std::sin(latitudeChange / halves)) +
        (std::cos(radians(fromLatitude)) * std::cos(radians(toLatitude)) *
         std::sin(longitudeChange / halves) * std::sin(longitudeChange / halves));
    return halves * earthRadiusMetres * std::asin(std::sqrt(std::fmin(1.0, halfChord)));
}

bool isPositionMasked(std::optional<HomeArea> home,
                      double latitudeDegrees,
                      double longitudeDegrees,
                      bool recording,
                      bool maskAlways) {
    if (!home.has_value() || (!recording && !maskAlways)) {
        return false;
    }
    return distanceMetres(
               home->latitudeDegrees, home->longitudeDegrees, latitudeDegrees, longitudeDegrees) <=
           home->radiusMetres;
}

MapViewModel::MapViewModel(std::optional<HomeArea> home,
                           bool maskAlways,
                           RecordingFunction recording,
                           QObject* parent)
    : QObject(parent), m_home(home), m_maskAlways(maskAlways), m_recording(std::move(recording)) {
    connect(&m_timer, &QTimer::timeout, this, &MapViewModel::refresh);
}

bool MapViewModel::hasPosition() const {
    return m_hasPosition;
}

bool MapViewModel::positionLive() const {
    return hasPosition() && m_live;
}

double MapViewModel::latitude() const {
    return m_masked ? 0.0 : m_latitude;
}

double MapViewModel::longitude() const {
    return m_masked ? 0.0 : m_longitude;
}

double MapViewModel::accuracyMetres() const {
    return m_accuracyMetres;
}

bool MapViewModel::masked() const {
    return m_masked;
}

QString MapViewModel::statusText() const {
    if (!hasPosition()) {
        return QStringLiteral("Waiting for the phone: open the phone link in Safari");
    }
    if (m_masked) {
        return QStringLiteral("Position hidden near home");
    }
    if (!m_live) {
        return QStringLiteral("Phone position lost: keep the page open on the phone");
    }
    return QStringLiteral("Phone position, %1 m").arg(std::lround(m_accuracyMetres));
}

void MapViewModel::refresh() {
    update();
}

void MapViewModel::startRefreshing(int intervalMilliseconds) {
    m_timer.start(intervalMilliseconds);
}

void MapViewModel::onSample(lexus_head_unit::SignalSample sample) {
    if (sample.status == SignalStatus::NeverReceived) {
        return;
    }
    // The service publishes latitude, longitude, then accuracy for every report (DN-053). The
    // position is taken over only with the accuracy, the last of the three, so the screen never
    // shows a new latitude with the old longitude, and the mask is decided on a whole position.
    switch (sample.signalId) {
    case SignalId::Latitude:
        m_pendingLatitude = sample.value;
        m_pendingParts |= latitudePart;
        return;
    case SignalId::Longitude:
        m_pendingLongitude = sample.value;
        m_pendingParts |= longitudePart;
        return;
    case SignalId::PositionAccuracy:
        if (m_pendingParts != (latitudePart | longitudePart) && !m_hasPosition) {
            return;
        }
        if (m_pendingParts == (latitudePart | longitudePart)) {
            m_latitude = m_pendingLatitude;
            m_longitude = m_pendingLongitude;
        }
        m_pendingParts = 0;
        m_accuracyMetres = sample.value;
        m_live = sample.status == SignalStatus::Valid;
        m_hasPosition = true;
        break;
    default:
        return;
    }
    update();
}

void MapViewModel::update() {
    const bool recording = m_recording ? m_recording() : false;
    // Hidden until both coordinates are known, so a half-updated position never shows.
    const bool masked =
        hasPosition() && isPositionMasked(m_home, m_latitude, m_longitude, recording, m_maskAlways);
    m_masked = masked;
    emit changed();
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
