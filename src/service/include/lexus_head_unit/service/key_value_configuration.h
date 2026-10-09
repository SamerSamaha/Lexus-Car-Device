#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace lexus_head_unit {

// A plain "key = value" file with "[section]" headers and "#" comments. Keys are looked up as
// "section.key"; keys before any section header have no prefix.
class KeyValueConfiguration {
public:
    bool loadFromFile(const std::string& path);
    void loadFromText(std::string_view text);

    [[nodiscard]] bool contains(const std::string& key) const;
    [[nodiscard]] std::string stringValue(const std::string& key,
                                          const std::string& defaultValue) const;
    [[nodiscard]] std::int64_t integerValue(const std::string& key,
                                            std::int64_t defaultValue) const;
    [[nodiscard]] const std::map<std::string, std::string>& entries() const;

private:
    std::map<std::string, std::string> m_entries;
};

} // namespace lexus_head_unit
