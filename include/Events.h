#pragma once

#include <atomic>
#include <optional>
#include <vector>

namespace Events {
    class EaseDiseaseHandler : public RE::BSTEventSink<RE::TESActiveEffectApplyRemoveEvent>, public RE::BSTEventSink<RE::TESMagicEffectApplyEvent> {
    public:
        static EaseDiseaseHandler* GetSingleton();

        void ResolveForms();
        void Register();
        void StartWatcher();
        void Resync();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* a_event, RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*) override;
        RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override;

    private:
        struct EaseKeyword {
            const char* editorID;
            float multiplier;
        };

        EaseDiseaseHandler() = default;

        std::optional<float> GetEaseMultiplier(RE::ActiveEffect* a_effect) const;
        bool HasEaseKeyword(const RE::EffectSetting* a_effect) const;
        bool IsDiseaseEffect(RE::ActiveEffect* a_effect) const;
        float ComputeMultiplier() const;
        void ScaleEffect(RE::ActiveEffect* a_effect, float a_multiplier) const;
        void Revalidate();
        void QueueRevalidate();

        std::vector<EaseKeyword> _keywords;
        std::atomic<bool> _watch{false};
        float _lastMultiplier = 1.0f;
        bool _revalidateQueued = false;
    };

    void Install();
}
