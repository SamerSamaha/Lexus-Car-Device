#include "lexus_head_unit/hub/app_entry.h"

#include <string_view>

namespace lexus_head_unit {

std::string_view toString(AppKind kind) {
    switch (kind) {
    case AppKind::Native:
        return "native";
    case AppKind::Url:
        return "url";
    }
    return "unknown";
}

std::string_view toString(RestartPolicy policy) {
    switch (policy) {
    case RestartPolicy::Never:
        return "never";
    case RestartPolicy::OnFailure:
        return "on-failure";
    }
    return "unknown";
}

} // namespace lexus_head_unit
