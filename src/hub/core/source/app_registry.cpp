#include "lexus_head_unit/hub/app_registry.h"

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/command_line.h"
#include "lexus_head_unit/service/key_value_configuration.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::string_view urlPlaceholder = "{url}";

std::string trimmed(std::string_view text) {
    const std::string_view whitespace = " \t";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return std::string(text.substr(first, last - first + 1));
}

std::vector<std::string> splitList(std::string_view text) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? text.size() : comma;
        std::string item = trimmed(text.substr(start, end - start));
        if (!item.empty()) {
            items.push_back(std::move(item));
        }
        start = end + 1;
    }
    return items;
}

bool isValidIdentifier(std::string_view appId) {
    return !appId.empty() && std::all_of(appId.begin(), appId.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '_' || character == '-';
    });
}

std::string replaceAll(std::string text, std::string_view placeholder, std::string_view value) {
    std::size_t position = text.find(placeholder);
    while (position != std::string::npos) {
        text.replace(position, placeholder.size(), value);
        position = text.find(placeholder, position + value.size());
    }
    return text;
}

// The browser command is split first and the URL is put into each word afterwards, so that
// spaces or quotes in a URL can never change where one argument ends.
std::optional<std::vector<std::string>> browserArguments(const std::string& browserCommand,
                                                         const std::string& url) {
    std::optional<std::vector<std::string>> words = splitCommandLine(browserCommand);
    if (!words.has_value()) {
        return std::nullopt;
    }
    for (std::string& word : *words) {
        word = replaceAll(word, urlPlaceholder, url);
    }
    return words;
}

struct EntryParse {
    std::optional<AppEntry> entry;
    std::string error;
};

EntryParse failed(std::string error) {
    return EntryParse{std::nullopt, std::move(error)};
}

std::optional<std::vector<std::string>> argumentsFor(const KeyValueConfiguration& configuration,
                                                     const std::string& appId,
                                                     const std::string& browserCommand,
                                                     AppKind kind,
                                                     std::string& error) {
    const std::string prefix = "app." + appId + ".";
    const std::string quoted = "app '" + appId + "'";
    if (kind == AppKind::Native) {
        std::optional<std::vector<std::string>> arguments =
            splitCommandLine(configuration.stringValue(prefix + "command", ""));
        if (!arguments.has_value()) {
            error = quoted + " has an unclosed quote in its command";
        }
        return arguments;
    }
    const std::string url = configuration.stringValue(prefix + "url", "");
    if (url.empty()) {
        error = quoted + " is a url app with no url";
        return std::nullopt;
    }
    if (browserCommand.find(urlPlaceholder) == std::string::npos) {
        error = quoted + " needs hub.browser_command containing {url}";
        return std::nullopt;
    }
    std::optional<std::vector<std::string>> arguments = browserArguments(browserCommand, url);
    if (!arguments.has_value()) {
        error = "hub.browser_command has an unclosed quote";
    }
    return arguments;
}

EntryParse parseEntry(const KeyValueConfiguration& configuration,
                      const std::string& appId,
                      const std::string& browserCommand) {
    const std::string prefix = "app." + appId + ".";
    const std::string quoted = "app '" + appId + "'";
    AppEntry entry;
    entry.id = appId;
    entry.name = configuration.stringValue(prefix + "name", "");
    entry.icon = configuration.stringValue(prefix + "icon", "");
    if (entry.name.empty()) {
        return failed(quoted + " has no name");
    }
    const std::string restart = configuration.stringValue(prefix + "restart", "never");
    if (restart == "never") {
        entry.restart = RestartPolicy::Never;
    } else if (restart == "on-failure") {
        entry.restart = RestartPolicy::OnFailure;
    } else {
        return failed(quoted + " has an unknown restart policy '" + restart + "'");
    }
    const std::string kind = configuration.stringValue(prefix + "kind", "");
    if (kind == "native") {
        entry.kind = AppKind::Native;
    } else if (kind == "url") {
        entry.kind = AppKind::Url;
    } else {
        return failed(quoted + " has an unknown kind '" + kind + "' (native or url)");
    }
    std::string error;
    std::optional<std::vector<std::string>> arguments =
        argumentsFor(configuration, appId, browserCommand, entry.kind, error);
    if (!arguments.has_value()) {
        return failed(error);
    }
    if (arguments->empty()) {
        return failed(quoted + " has an empty command");
    }
    entry.arguments = std::move(*arguments);
    return EntryParse{std::move(entry), {}};
}

} // namespace

AppRegistry AppRegistry::fromConfiguration(const KeyValueConfiguration& configuration) {
    AppRegistry registry;
    const std::vector<std::string> appIds = splitList(configuration.stringValue("hub.apps", ""));
    if (appIds.empty()) {
        registry.m_errors.emplace_back("hub.apps is missing or empty");
        return registry;
    }
    const std::string browserCommand = configuration.stringValue("hub.browser_command", "");
    std::set<std::string> seenIds;
    for (const std::string& appId : appIds) {
        if (!isValidIdentifier(appId)) {
            registry.m_errors.push_back("app id '" + appId +
                                        "' may use only a to z, 0 to 9, _ and -");
            continue;
        }
        if (!seenIds.insert(appId).second) {
            registry.m_errors.push_back("app '" + appId + "' is listed twice in hub.apps");
            continue;
        }
        EntryParse parsed = parseEntry(configuration, appId, browserCommand);
        if (!parsed.entry.has_value()) {
            registry.m_errors.push_back(std::move(parsed.error));
            continue;
        }
        registry.m_entries.push_back(std::move(*parsed.entry));
    }
    return registry;
}

const std::vector<AppEntry>& AppRegistry::entries() const {
    return m_entries;
}

const std::vector<std::string>& AppRegistry::errors() const {
    return m_errors;
}

const AppEntry* AppRegistry::find(std::string_view appId) const {
    const auto found =
        std::find_if(m_entries.begin(), m_entries.end(), [appId](const AppEntry& entry) {
            return entry.id == appId;
        });
    return found == m_entries.end() ? nullptr : &*found;
}

} // namespace lexus_head_unit
