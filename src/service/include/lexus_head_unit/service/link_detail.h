#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace lexus_head_unit {

// What the driver needs to know about the link, beside the four connection states (DN-042,
// REQ-023). The state machine keeps exactly its four states (REQ-007); a source that can tell
// these situations apart reports the detail, and every other source gets the mapping of
// linkDetailForState().
enum class LinkDetail {
    Idle,
    SearchingForAdapter,
    AdapterWithoutVehicle,
    Live,
    LinkLostRetrying,
};

constexpr std::size_t linkDetailCount = 5;

constexpr std::array<LinkDetail, linkDetailCount> allLinkDetails = {
    LinkDetail::Idle,
    LinkDetail::SearchingForAdapter,
    LinkDetail::AdapterWithoutVehicle,
    LinkDetail::Live,
    LinkDetail::LinkLostRetrying,
};

// The detail a source without its own knowledge reports for a state.
LinkDetail linkDetailForState(ConnectionState state);

// The detail to show after a transition, before the source reports its own: a handshake that
// failed has not found the adapter yet, a lost link is retrying, and Connecting keeps the
// previous detail so that a retry does not flicker.
LinkDetail linkDetailAfter(const ConnectionTransition& transition, LinkDetail previous);

// Enumeration number on the wire (D-Bus); empty when out of range.
std::optional<LinkDetail> linkDetailFromNumber(unsigned int number);

std::string_view toString(LinkDetail detail);
// The short text on the status strip.
std::string_view displayText(LinkDetail detail);

} // namespace lexus_head_unit
