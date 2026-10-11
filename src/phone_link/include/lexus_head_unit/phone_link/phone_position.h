#pragma once

#include "lexus_head_unit/service/signal_sample.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

// One position report from the phone (DN-053): what the browser's geolocation call returned.
struct PhonePosition {
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    double accuracyMetres = 0.0;
};

// The JSON body of POST /position: {"latitude": d, "longitude": d, "accuracy": d}, extra fields
// ignored. Nothing, with the reason in `error`, for anything else: not an object, a field
// missing or not a finite number, latitude outside -90 to 90, longitude outside -180 to 180,
// accuracy negative or above maximumAccuracyMetres.
std::optional<PhonePosition> parsePhonePosition(const QByteArray& body, QString& error);

constexpr double maximumAccuracyMetres = 100000.0;

// Turns accepted reports into the three phone signals and makes them Stale when the phone has
// been quiet for their staleness timeout (5 s, signal_definition.cpp). Plain logic on a given
// monotonic time, so the service's timer and the tests drive it the same way. Not thread-safe:
// the service uses it on its main thread only.
class PhonePositionTracker {
public:
    // Valid samples of latitude, longitude and accuracy at nowMilliseconds; nothing when the
    // time is not later than the last accepted report (the store's rule for one signal).
    std::vector<SignalSample> onPosition(const PhonePosition& position,
                                         std::int64_t nowMilliseconds);
    // The three samples again with status Stale, once, when the last report is older than the
    // timeout; nothing otherwise.
    std::vector<SignalSample> checkStaleness(std::int64_t nowMilliseconds);

    [[nodiscard]] std::uint64_t reportsAccepted() const;

private:
    std::vector<SignalSample> samplesWithStatus(SignalStatus status) const;

    PhonePosition m_last;
    std::int64_t m_lastMilliseconds = 0;
    bool m_hasReport = false;
    bool m_stale = false;
    std::uint64_t m_reportsAccepted = 0;
};

} // namespace lexus_head_unit

Q_DECLARE_METATYPE(lexus_head_unit::PhonePosition)
