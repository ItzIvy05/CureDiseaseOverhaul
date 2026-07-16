 #pragma once
 
namespace Settings {
    inline constexpr const char* INI_PATH = "Data/SKSE/Plugins/CureDiseaseOverhaul.ini";
 
    struct EaseEntry {
        const char* keywordEditorID;
        float reductionPercent;
    };
 
    inline constexpr const char* EASE_KEYWORD = "CDOEaseDiseaseKeyword";
    inline constexpr const char* SHRINE_KEYWORD = "CDOShrineEaseDiseaseKeyword";
    inline constexpr float DEFAULT_EASE_REDUCTION = 50.0f;
    inline constexpr float DEFAULT_SHRINE_REDUCTION = 80.0f;
 
    inline EaseEntry g_entries[2] = {{EASE_KEYWORD, DEFAULT_EASE_REDUCTION}, {SHRINE_KEYWORD, DEFAULT_SHRINE_REDUCTION}};
    inline bool g_enableLogging = true;
    inline bool g_iniFound = false;
 
    void Load();
    void LogSummary();
}