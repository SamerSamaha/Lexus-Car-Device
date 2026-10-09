#pragma once

#include "lexus_head_unit/service/link_detail.h"

#include <cstdint>
#include <string_view>

namespace lexus_head_unit {

// DN-043: the hub shuts the Pi down cleanly after the car has been switched off, before anyone
// pulls the power bank cable. Times in milliseconds of a monotonic clock.
struct IgnitionOffShutdownSettings {
    // Quiet time (no live vehicle data) before the countdown; 0 turns the policy off.
    std::int64_t quietMilliseconds = 300000;
    // The countdown shown on screen, with Cancel, before the shutdown command runs.
    std::int64_t countdownMilliseconds = 60000;
};

enum class IgnitionOffShutdownPhase {
    Disabled,
    // Not yet live in this run: a desk session, a demo or a car never switched on stays here.
    WaitingForLive,
    Live,
    Quiet,
    CountingDown,
    // Returned by update() once, when the countdown has run out.
    ShutdownDue,
    // After a shutdown was due; the policy does nothing more.
    Done,
    // The countdown was cancelled; it does not return until the vehicle is live again.
    Cancelled,
};

// Plain C++ with no clock of its own: the caller passes the time, so tests drive it exactly.
class IgnitionOffShutdownPolicy {
public:
    explicit IgnitionOffShutdownPolicy(IgnitionOffShutdownSettings settings);

    // Live arms the policy and ends any quiet time or countdown. AdapterWithoutVehicle (the
    // ignition is off), LinkLostRetrying (the adapter went to sleep) and Idle (the service
    // stopped) count as quiet; SearchingForAdapter never follows Live and is ignored.
    void onLinkDetail(LinkDetail detail, std::int64_t nowMilliseconds);
    // Advances the phase to the time given and returns it.
    IgnitionOffShutdownPhase update(std::int64_t nowMilliseconds);
    // Only during the countdown.
    void cancel();

    [[nodiscard]] IgnitionOffShutdownPhase phase() const;
    // Of the countdown; 0 outside it.
    [[nodiscard]] std::int64_t millisecondsRemaining(std::int64_t nowMilliseconds) const;
    [[nodiscard]] const IgnitionOffShutdownSettings& settings() const;

private:
    IgnitionOffShutdownSettings m_settings;
    IgnitionOffShutdownPhase m_phase;
    std::int64_t m_quietSinceMilliseconds = 0;
    std::int64_t m_countdownEndsAtMilliseconds = 0;
};

std::string_view toString(IgnitionOffShutdownPhase phase);

} // namespace lexus_head_unit
