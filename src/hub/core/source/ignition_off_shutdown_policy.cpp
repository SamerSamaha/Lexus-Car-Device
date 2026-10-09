#include "lexus_head_unit/hub/ignition_off_shutdown_policy.h"

#include "lexus_head_unit/service/link_detail.h"

#include <cstdint>
#include <string_view>

namespace lexus_head_unit {

IgnitionOffShutdownPolicy::IgnitionOffShutdownPolicy(IgnitionOffShutdownSettings settings)
    : m_settings(settings),
      m_phase(settings.quietMilliseconds > 0 ? IgnitionOffShutdownPhase::WaitingForLive
                                             : IgnitionOffShutdownPhase::Disabled) {}

void IgnitionOffShutdownPolicy::onLinkDetail(LinkDetail detail, std::int64_t nowMilliseconds) {
    if (m_phase == IgnitionOffShutdownPhase::Disabled ||
        m_phase == IgnitionOffShutdownPhase::Done) {
        return;
    }
    switch (detail) {
    case LinkDetail::Live:
        m_phase = IgnitionOffShutdownPhase::Live;
        return;
    case LinkDetail::AdapterWithoutVehicle:
    case LinkDetail::LinkLostRetrying:
    case LinkDetail::Idle:
        if (m_phase == IgnitionOffShutdownPhase::Live) {
            m_phase = IgnitionOffShutdownPhase::Quiet;
            m_quietSinceMilliseconds = nowMilliseconds;
        }
        return;
    case LinkDetail::SearchingForAdapter:
        return;
    }
}

IgnitionOffShutdownPhase IgnitionOffShutdownPolicy::update(std::int64_t nowMilliseconds) {
    if (m_phase == IgnitionOffShutdownPhase::Quiet &&
        nowMilliseconds - m_quietSinceMilliseconds >= m_settings.quietMilliseconds) {
        m_phase = IgnitionOffShutdownPhase::CountingDown;
        m_countdownEndsAtMilliseconds = nowMilliseconds + m_settings.countdownMilliseconds;
    }
    if (m_phase == IgnitionOffShutdownPhase::CountingDown &&
        nowMilliseconds >= m_countdownEndsAtMilliseconds) {
        m_phase = IgnitionOffShutdownPhase::Done;
        return IgnitionOffShutdownPhase::ShutdownDue;
    }
    return m_phase;
}

void IgnitionOffShutdownPolicy::cancel() {
    if (m_phase == IgnitionOffShutdownPhase::CountingDown) {
        m_phase = IgnitionOffShutdownPhase::Cancelled;
    }
}

IgnitionOffShutdownPhase IgnitionOffShutdownPolicy::phase() const {
    return m_phase;
}

std::int64_t IgnitionOffShutdownPolicy::millisecondsRemaining(std::int64_t nowMilliseconds) const {
    if (m_phase != IgnitionOffShutdownPhase::CountingDown) {
        return 0;
    }
    const std::int64_t remaining = m_countdownEndsAtMilliseconds - nowMilliseconds;
    return remaining > 0 ? remaining : 0;
}

const IgnitionOffShutdownSettings& IgnitionOffShutdownPolicy::settings() const {
    return m_settings;
}

std::string_view toString(IgnitionOffShutdownPhase phase) {
    switch (phase) {
    case IgnitionOffShutdownPhase::Disabled:
        return "Disabled";
    case IgnitionOffShutdownPhase::WaitingForLive:
        return "WaitingForLive";
    case IgnitionOffShutdownPhase::Live:
        return "Live";
    case IgnitionOffShutdownPhase::Quiet:
        return "Quiet";
    case IgnitionOffShutdownPhase::CountingDown:
        return "CountingDown";
    case IgnitionOffShutdownPhase::ShutdownDue:
        return "ShutdownDue";
    case IgnitionOffShutdownPhase::Done:
        return "Done";
    case IgnitionOffShutdownPhase::Cancelled:
        return "Cancelled";
    }
    return "UnknownPhase";
}

} // namespace lexus_head_unit
