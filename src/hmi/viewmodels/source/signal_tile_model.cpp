#include "lexus_head_unit/hmi/signal_tile_model.h"

#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>

#include <string_view>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr int oneDecimalPlace = 1;

QString textOf(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

SignalTileModel::SignalTileModel(SignalId signalId, QObject* parent)
    : QObject(parent),
      m_signalId(signalId),
      m_unit(definitionOf(signalId).unit),
      m_name(textOf(definitionOf(signalId).name)),
      m_unitText(textOf(toString(m_unit))),
      m_valueText(QStringLiteral("--")) {}

SignalId SignalTileModel::signalId() const {
    return m_signalId;
}

QString SignalTileModel::name() const {
    return m_name;
}

QString SignalTileModel::unitText() const {
    return m_unitText;
}

QString SignalTileModel::valueText() const {
    return m_valueText;
}

QString SignalTileModel::statusText() const {
    return textOf(toString(m_status));
}

bool SignalTileModel::isValid() const {
    return m_status == SignalStatus::Valid;
}

bool SignalTileModel::isStale() const {
    return m_status == SignalStatus::Stale;
}

bool SignalTileModel::isNeverReceived() const {
    return m_status == SignalStatus::NeverReceived;
}

void SignalTileModel::applySample(const SignalSample& sample) {
    if (sample.signalId != m_signalId) {
        return;
    }
    m_status = sample.status;
    if (m_status == SignalStatus::NeverReceived) {
        m_valueText = QStringLiteral("--");
    } else {
        m_valueText = formatValue(sample.value, m_unit);
    }
    emit changed();
}

QString SignalTileModel::formatValue(double value, Unit unit) {
    const bool oneDecimal = unit == Unit::Volts || unit == Unit::GramsPerSecond ||
                            unit == Unit::LitresPer100Kilometres || unit == Unit::Kilometres ||
                            unit == Unit::Minutes;
    const int decimals = oneDecimal ? oneDecimalPlace : 0;
    return QString::number(value, 'f', decimals);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
