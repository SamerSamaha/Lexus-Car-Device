#pragma once

#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <optional>

namespace lexus_head_unit {

// The owner's home, from car.conf on the Pi only ([privacy] in DN-054): never in the repository.
struct HomeArea {
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    double radiusMetres = 0.0;
};

// Great-circle distance in metres between two positions in degrees (haversine, Earth radius
// 6,371,000 m: within 0.5 % of the ellipsoid, plenty for a privacy radius).
double
distanceMetres(double fromLatitude, double fromLongitude, double toLatitude, double toLongitude);

// Whether a position must be hidden (DN-054, REQ-030): inside the home area while the screen is
// recorded, or always when maskAlways is set. Without a home area nothing is hidden.
bool isPositionMasked(std::optional<HomeArea> home,
                      double latitudeDegrees,
                      double longitudeDegrees,
                      bool recording,
                      bool maskAlways);

// What the map screen binds to: the phone's position from the vehicle-data service, its age,
// and whether it is hidden. Fed the samples of the three phone signals (other signals are
// ignored); `recording` comes from a function so the app can pass the recorder's status file.
class MapViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY changed)
    Q_PROPERTY(bool positionLive READ positionLive NOTIFY changed)
    Q_PROPERTY(double latitude READ latitude NOTIFY changed)
    Q_PROPERTY(double longitude READ longitude NOTIFY changed)
    Q_PROPERTY(double accuracyMetres READ accuracyMetres NOTIFY changed)
    Q_PROPERTY(bool masked READ masked NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)

public:
    using RecordingFunction = std::function<bool()>;

    MapViewModel(std::optional<HomeArea> home,
                 bool maskAlways,
                 RecordingFunction recording,
                 QObject* parent = nullptr);

    // A position has arrived at least once (Valid or Stale).
    [[nodiscard]] bool hasPosition() const;
    // The latest position is Valid (not older than 5 s at the service).
    [[nodiscard]] bool positionLive() const;
    [[nodiscard]] double latitude() const;
    [[nodiscard]] double longitude() const;
    [[nodiscard]] double accuracyMetres() const;
    [[nodiscard]] bool masked() const;
    // "Waiting for the phone: open the phone link in Safari", "Phone position, 6 m",
    // "Phone position lost: keep the page open", "Position hidden near home".
    [[nodiscard]] QString statusText() const;

    // Re-reads the recording state (the app calls it every second).
    Q_INVOKABLE void refresh();
    void startRefreshing(int intervalMilliseconds);

public slots:
    void onSample(lexus_head_unit::SignalSample sample);

signals:
    void changed();

private:
    void update();

    std::optional<HomeArea> m_home;
    bool m_maskAlways;
    RecordingFunction m_recording;
    QTimer m_timer;
    static constexpr unsigned latitudePart = 1U;
    static constexpr unsigned longitudePart = 2U;

    double m_latitude = 0.0;
    double m_longitude = 0.0;
    double m_accuracyMetres = 0.0;
    double m_pendingLatitude = 0.0;
    double m_pendingLongitude = 0.0;
    unsigned m_pendingParts = 0;
    bool m_hasPosition = false;
    bool m_live = false;
    bool m_masked = false;
};

} // namespace lexus_head_unit
