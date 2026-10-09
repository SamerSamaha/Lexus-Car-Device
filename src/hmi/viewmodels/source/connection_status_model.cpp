#include "lexus_head_unit/hmi/connection_status_model.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"

#include <QObject>
#include <QString>

#include <string_view>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

QString textOf(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

ConnectionStatusModel::ConnectionStatusModel(QObject* parent) : QObject(parent) {}

ConnectionState ConnectionStatusModel::state() const {
    return m_state;
}

QString ConnectionStatusModel::stateText() const {
    return textOf(toString(m_state));
}

QString ConnectionStatusModel::lastCauseText() const {
    return m_lastCauseText;
}

bool ConnectionStatusModel::isConnected() const {
    return m_state == ConnectionState::Connected;
}

bool ConnectionStatusModel::isError() const {
    return m_state == ConnectionState::Error;
}

quint64 ConnectionStatusModel::transitionCount() const {
    return m_transitionCount;
}

LinkDetail ConnectionStatusModel::linkDetail() const {
    return m_linkDetail;
}

QString ConnectionStatusModel::detailText() const {
    return textOf(displayText(m_linkDetail));
}

QString ConnectionStatusModel::detailName() const {
    return textOf(toString(m_linkDetail));
}

void ConnectionStatusModel::applyTransition(const ConnectionTransition& transition) {
    m_state = transition.to;
    m_lastCauseText = textOf(toString(transition.trigger));
    m_linkDetail = linkDetailAfter(transition, m_linkDetail);
    ++m_transitionCount;
    emit changed();
}

void ConnectionStatusModel::applyLinkDetail(LinkDetail detail) {
    if (detail == m_linkDetail) {
        return;
    }
    m_linkDetail = detail;
    emit changed();
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
