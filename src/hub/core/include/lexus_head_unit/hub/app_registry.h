#pragma once

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/service/key_value_configuration.h"

#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

// The hub's apps, read from the registry file (DN-021):
//   [hub]  apps = id, id, ...   browser_command = chromium ... {url}
//   [app.<id>]  name, kind (native | url), command or url, icon, restart (never | on-failure)
// A bad entry is skipped and described in errors(); the other entries still load.
class AppRegistry {
public:
    static AppRegistry fromConfiguration(const KeyValueConfiguration& configuration);

    [[nodiscard]] const std::vector<AppEntry>& entries() const;
    [[nodiscard]] const std::vector<std::string>& errors() const;
    [[nodiscard]] const AppEntry* find(std::string_view appId) const;

private:
    std::vector<AppEntry> m_entries;
    std::vector<std::string> m_errors;
};

} // namespace lexus_head_unit
