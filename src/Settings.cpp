#include "Settings.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <string>
#include <unordered_map>

namespace Settings {
    namespace {
        void Trim(std::string& value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                value.clear();
                return;
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            value = value.substr(first, last - first + 1);
        }

        std::string LowerCopy(std::string value) {
            for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        }
    }

    void Load() {
        
        g_entries[0].reductionPercent = DEFAULT_EASE_REDUCTION;
        g_entries[1].reductionPercent = DEFAULT_SHRINE_REDUCTION;
        g_enableLogging = true;

        std::ifstream file(INI_PATH);
        g_iniFound = static_cast<bool>(file);
        if (!g_iniFound) return;

        std::unordered_map<std::string, std::string> values;
        std::string line;
        
        while (std::getline(file, line)) {
            Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            auto key = line.substr(0, eq);
            auto value = line.substr(eq + 1);
            Trim(key);
            Trim(value);
            values[key] = value;
        }

        const auto readReduction = [&](EaseEntry& entry, const char* key) {
            const auto it = values.find(key);
            if (it == values.end()) return;
            float parsed = entry.reductionPercent;
            const auto& raw = it->second;
            if (std::from_chars(raw.data(), raw.data() + raw.size(), parsed).ec == std::errc{}) {
                entry.reductionPercent = std::clamp(parsed, 0.0f, 100.0f);
            }
        };

        readReduction(g_entries[0], "EaseDiseaseReduction");
        readReduction(g_entries[1], "ShrineEaseDiseaseReduction");

        if (const auto it = values.find("EnableLogging"); it != values.end()) {
            const auto value = LowerCopy(it->second);
            g_enableLogging = value == "true" || value == "1";
        }
    }

    void LogSummary() {
        if (!g_iniFound) {
            logger::warn("No INI found at {}, using defaults.", INI_PATH);
        }
        for (const auto& entry : g_entries) {
            logger::info("'{}' reduces disease magnitude by {}%.", entry.keywordEditorID, entry.reductionPercent);
        }
    }
}