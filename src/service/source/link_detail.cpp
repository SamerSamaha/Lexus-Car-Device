#include "lexus_head_unit/service/link_detail.h"

#include "lexus_head_unit/service/connection_state_machine.h"

#include <optional>
#include <string_view>

namespace lexus_head_unit {

LinkDetail linkDetailForState(ConnectionState state) {
    switch (state) {
    case ConnectionState::Disconnected:
        return LinkDetail::Idle;
    case ConnectionState::Connecting:
        return LinkDetail::SearchingForAdapter;
    case ConnectionState::Connected:
        return LinkDetail::Live;
    case ConnectionState::Error:
        return LinkDetail::LinkLostRetrying;
    }
    return LinkDetail::Idle;
}

LinkDetail linkDetailAfter(const ConnectionTransition& transition, LinkDetail previous) {
    switch (transition.to) {
    case ConnectionState::Connecting:
        return previous == LinkDetail::Idle ? LinkDetail::SearchingForAdapter : previous;
    case ConnectionState::Error:
        return transition.trigger == ConnectionTrigger::HandshakeFailed
                   ? LinkDetail::SearchingForAdapter
                   : LinkDetail::LinkLostRetrying;
    case ConnectionState::Connected:
    case ConnectionState::Disconnected:
        return linkDetailForState(transition.to);
    }
    return previous;
}

std::optional<LinkDetail> linkDetailFromNumber(unsigned int number) {
    if (number >= linkDetailCount) {
        return std::nullopt;
    }
    return allLinkDetails.at(number);
}

std::string_view toString(LinkDetail detail) {
    switch (detail) {
    case LinkDetail::Idle:
        return "Idle";
    case LinkDetail::SearchingForAdapter:
        return "SearchingForAdapter";
    case LinkDetail::AdapterWithoutVehicle:
        return "AdapterWithoutVehicle";
    case LinkDetail::Live:
        return "Live";
    case LinkDetail::LinkLostRetrying:
        return "LinkLostRetrying";
    }
    return "UnknownLinkDetail";
}

std::string_view displayText(LinkDetail detail) {
    switch (detail) {
    case LinkDetail::Idle:
        return "Not started";
    case LinkDetail::SearchingForAdapter:
        return "Searching for adapter";
    case LinkDetail::AdapterWithoutVehicle:
        return "Adapter found, no vehicle";
    case LinkDetail::Live:
        return "Live";
    case LinkDetail::LinkLostRetrying:
        return "Link lost, retrying";
    }
    return "";
}

} // namespace lexus_head_unit
