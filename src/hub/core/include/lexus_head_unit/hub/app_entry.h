#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

enum class AppKind {
    Native,
    Url,
};

enum class RestartPolicy {
    Never,
    OnFailure,
};

// One app of the hub's registry. arguments is the argument vector the hub starts, already split
// and, for a URL app, with the URL put into the browser command.
struct AppEntry {
    std::string id;
    std::string name;
    std::string icon;
    AppKind kind = AppKind::Native;
    std::vector<std::string> arguments;
    RestartPolicy restart = RestartPolicy::Never;
};

std::string_view toString(AppKind kind);
std::string_view toString(RestartPolicy policy);

} // namespace lexus_head_unit
