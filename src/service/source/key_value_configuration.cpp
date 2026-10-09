#include "lexus_head_unit/service/key_value_configuration.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace lexus_head_unit {

namespace {

constexpr int decimalBase = 10;

std::string trimmed(std::string_view text) {
    const std::string_view whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return std::string(text.substr(first, last - first + 1));
}

} // namespace

bool KeyValueConfiguration::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream contents;
    contents << file.rdbuf();
    loadFromText(contents.str());
    return true;
}

void KeyValueConfiguration::loadFromText(std::string_view text) {
    std::string section;
    std::size_t lineStart = 0;
    while (lineStart <= text.size()) {
        const std::size_t lineEnd = text.find('\n', lineStart);
        const std::string_view rawLine = text.substr(
            lineStart,
            lineEnd == std::string_view::npos ? std::string_view::npos : lineEnd - lineStart);
        lineStart = lineEnd == std::string_view::npos ? text.size() + 1 : lineEnd + 1;
        const std::string line = trimmed(rawLine);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trimmed(std::string_view(line).substr(1, line.size() - 2));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trimmed(std::string_view(line).substr(0, equals));
        const std::string value = trimmed(std::string_view(line).substr(equals + 1));
        if (key.empty()) {
            continue;
        }
        std::string fullKey = section;
        if (!fullKey.empty()) {
            fullKey += '.';
        }
        fullKey += key;
        m_entries[fullKey] = value;
    }
}

void KeyValueConfiguration::setValue(const std::string& key, const std::string& value) {
    m_entries[key] = value;
}

bool KeyValueConfiguration::contains(const std::string& key) const {
    return m_entries.find(key) != m_entries.end();
}

std::string KeyValueConfiguration::stringValue(const std::string& key,
                                               const std::string& defaultValue) const {
    const auto entry = m_entries.find(key);
    return entry == m_entries.end() ? defaultValue : entry->second;
}

std::int64_t KeyValueConfiguration::integerValue(const std::string& key,
                                                 std::int64_t defaultValue) const {
    const auto entry = m_entries.find(key);
    if (entry == m_entries.end() || entry->second.empty()) {
        return defaultValue;
    }
    const std::string& text = entry->second;
    std::size_t consumed = 0;
    try {
        const long long parsed = std::stoll(text, &consumed, decimalBase);
        if (consumed != text.size()) {
            return defaultValue;
        }
        return static_cast<std::int64_t>(parsed);
    } catch (const std::exception&) {
        return defaultValue;
    }
}

const std::map<std::string, std::string>& KeyValueConfiguration::entries() const {
    return m_entries;
}

} // namespace lexus_head_unit
