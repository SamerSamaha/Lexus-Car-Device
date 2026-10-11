#include "lexus_head_unit/phone_link/phone_position.h"

#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr double maximumLatitudeDegrees = 90.0;
constexpr double maximumLongitudeDegrees = 180.0;

std::optional<double> finiteNumber(const QJsonObject& object, const QString& key) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        return std::nullopt;
    }
    return value.toDouble();
}

SignalSample
sampleOf(SignalId signalId, double value, std::int64_t timestampMilliseconds, SignalStatus status) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.value = value;
    sample.unit = definitionOf(signalId).unit;
    sample.timestampMilliseconds = timestampMilliseconds;
    sample.status = status;
    return sample;
}

} // namespace

std::optional<PhonePosition> parsePhonePosition(const QByteArray& body, QString& error) {
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("not a JSON object");
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    const std::optional<double> latitude = finiteNumber(object, QStringLiteral("latitude"));
    const std::optional<double> longitude = finiteNumber(object, QStringLiteral("longitude"));
    const std::optional<double> accuracy = finiteNumber(object, QStringLiteral("accuracy"));
    if (!latitude.has_value() || !longitude.has_value() || !accuracy.has_value()) {
        error = QStringLiteral("latitude, longitude and accuracy must be numbers");
        return std::nullopt;
    }
    if (std::fabs(*latitude) > maximumLatitudeDegrees ||
        std::fabs(*longitude) > maximumLongitudeDegrees) {
        error = QStringLiteral("position out of range");
        return std::nullopt;
    }
    if (*accuracy < 0.0 || *accuracy > maximumAccuracyMetres) {
        error = QStringLiteral("accuracy out of range");
        return std::nullopt;
    }
    return PhonePosition{*latitude, *longitude, *accuracy};
}

std::vector<SignalSample> PhonePositionTracker::onPosition(const PhonePosition& position,
                                                           std::int64_t nowMilliseconds) {
    if (m_hasReport && nowMilliseconds <= m_lastMilliseconds) {
        return {};
    }
    m_last = position;
    m_lastMilliseconds = nowMilliseconds;
    m_hasReport = true;
    m_stale = false;
    ++m_reportsAccepted;
    return samplesWithStatus(SignalStatus::Valid);
}

std::vector<SignalSample> PhonePositionTracker::checkStaleness(std::int64_t nowMilliseconds) {
    const std::int64_t timeout = definitionOf(SignalId::Latitude).stalenessTimeoutMilliseconds;
    if (!m_hasReport || m_stale || nowMilliseconds - m_lastMilliseconds <= timeout) {
        return {};
    }
    m_stale = true;
    return samplesWithStatus(SignalStatus::Stale);
}

std::uint64_t PhonePositionTracker::reportsAccepted() const {
    return m_reportsAccepted;
}

std::vector<SignalSample> PhonePositionTracker::samplesWithStatus(SignalStatus status) const {
    return {
        sampleOf(SignalId::Latitude, m_last.latitudeDegrees, m_lastMilliseconds, status),
        sampleOf(SignalId::Longitude, m_last.longitudeDegrees, m_lastMilliseconds, status),
        sampleOf(SignalId::PositionAccuracy, m_last.accuracyMetres, m_lastMilliseconds, status)};
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
