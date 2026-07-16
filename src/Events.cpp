#include "Events.h"
#include "Settings.h"
#include <chrono>
#include <cmath>
#include <thread>

namespace Events {
    namespace {
        bool SafeModifyActorValue(RE::ValueModifierEffect* a_effect, RE::Actor* a_actor, float a_delta, RE::ActorValue a_actorValue) noexcept {
            __try {
                a_effect->ModifyActorValue(a_actor, a_delta, a_actorValue);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        RE::ActiveEffect* FindActiveEffect(RE::Actor* a_actor, std::uint16_t a_uniqueID) {
            auto* magicTarget = a_actor ? a_actor->GetMagicTarget() : nullptr;
            auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
            if (!effects) return nullptr;

            for (auto* effect : *effects) {
                if (effect && effect->usUniqueID == a_uniqueID) return effect;
            }
            return nullptr;
        }
    }

    EaseDiseaseHandler* EaseDiseaseHandler::GetSingleton() {
        static EaseDiseaseHandler singleton;
        return &singleton;
    }

    void EaseDiseaseHandler::ResolveForms() {
        _keywords.clear();
        for (const auto& entry : Settings::g_entries) {
            const float multiplier = (100.0f - entry.reductionPercent) / 100.0f;
            _keywords.push_back({entry.keywordEditorID, multiplier});
            logger::info("Matching keyword '{}' by editor ID -> magnitude x{}.", entry.keywordEditorID, multiplier);

            if (auto* keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(entry.keywordEditorID)) {
                logger::info("  (Keyword form with this editor ID exists: {:08X})", keyword->GetFormID());
            } 
           
            else {
                logger::warn("  (No keyword form with this editor ID found, ESP Missing my guy?)");
            }
        }
    }

    void EaseDiseaseHandler::Register() {
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESActiveEffectApplyRemoveEvent>(this);
            holder->AddEventSink<RE::TESMagicEffectApplyEvent>(this);
            logger::info("Registered active effect apply/remove + magic effect apply sinks.");
        }
    }

    void EaseDiseaseHandler::StartWatcher() {
        static std::atomic<bool> started{false};
        if (started.exchange(true)) return;

        std::thread([this]() {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (_watch.load(std::memory_order_relaxed)) {
                    SKSE::GetTaskInterface()->AddTask([this]() { Revalidate(); });
                }
            }
        }).detach();
    }

    bool EaseDiseaseHandler::HasEaseKeyword(const RE::EffectSetting* a_effect) const {
        if (!a_effect) return false;
        for (const auto& entry : _keywords) {
            if (a_effect->HasKeywordString(entry.editorID)) return true;
        }
        return false;
    }

    std::optional<float> EaseDiseaseHandler::GetEaseMultiplier(RE::ActiveEffect* a_effect) const {
        if (_keywords.empty() || !a_effect) return std::nullopt;

        auto* base = a_effect->GetBaseObject();
        for (const auto& entry : _keywords) {
            if (base && base->HasKeywordString(entry.editorID)) return entry.multiplier;
            if (a_effect->spell && a_effect->spell->HasKeywordString(entry.editorID)) return entry.multiplier;
        }
        return std::nullopt;
    }

    bool EaseDiseaseHandler::IsDiseaseEffect(RE::ActiveEffect* a_effect) const {
        return a_effect && a_effect->spell &&
               a_effect->spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;
    }

    float EaseDiseaseHandler::ComputeMultiplier() const {
        float multiplier = 1.0f;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return multiplier;

        auto* magicTarget = player->GetMagicTarget();
        if (auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr) {
            for (auto* effect : *effects) {
                if (const auto value = GetEaseMultiplier(effect)) multiplier = std::min(multiplier, *value);
            }
        }

        const auto checkSpell = [&](RE::SpellItem* a_spell) {
            if (!a_spell) return;
            for (const auto& entry : _keywords) {
                if (a_spell->HasKeywordString(entry.editorID)) {
                    multiplier = std::min(multiplier, entry.multiplier);
                    return;
                }
            }
            for (const auto* effect : a_spell->effects) {
                if (!effect || !effect->baseEffect) continue;
                for (const auto& entry : _keywords) {
                    if (effect->baseEffect->HasKeywordString(entry.editorID)) {
                        multiplier = std::min(multiplier, entry.multiplier);
                    }
                }
            }
        };

        for (auto* spell : player->GetActorRuntimeData().addedSpells) checkSpell(spell);
        if (auto* actorBase = player->GetActorBase()) {
            if (const auto* spellData = actorBase->actorEffects) {
                for (std::uint32_t i = 0; i < spellData->numSpells; ++i) checkSpell(spellData->spells[i]);
            }
        }

        return multiplier;
    }

    void EaseDiseaseHandler::ScaleEffect(RE::ActiveEffect* a_effect, float a_multiplier) const {
        if (!a_effect) return;

        using Flag = RE::ActiveEffect::Flag;
        if (a_effect->flags.any(Flag::kDispelled, Flag::kInactive)) return;

        auto* base = a_effect->GetBaseObject();

        using Archetype = RE::EffectArchetypes::ArchetypeID;
        const auto archetype = base ? base->GetArchetype() : Archetype::kNone;

        const auto* item = a_effect->effect;
        const float baseMag = item ? std::fabs(item->GetMagnitude()) : 0.0f;
        if (baseMag <= 0.0f) return;

        const float oldMag = a_effect->magnitude;
        const float magSign = std::signbit(oldMag) ? -1.0f : 1.0f;
        const float newMag = magSign * baseMag * a_multiplier;
        if (newMag == oldMag) return;

        if (archetype == Archetype::kValueModifier) {
            if (auto* valMod = skyrim_cast<RE::ValueModifierEffect*>(a_effect)) {
                auto* actor = valMod->GetTargetActor();
                const float oldValue = valMod->value;
                const bool valueApplied = oldValue != 0.0f || std::signbit(oldValue);
                if (actor && valueApplied && valMod->actorValue != RE::ActorValue::kNone) {
                    const float valSign = std::signbit(oldValue) ? -1.0f : 1.0f;
                    const float newValue = valSign * baseMag * a_multiplier;
                    if (SafeModifyActorValue(valMod, actor, newValue - oldValue, valMod->actorValue)) {
                        valMod->value = newValue;
                    } else {
                        logger::warn("Guarded againt a reapply when game does not have that stage '{}', stat left for natural reapply.",
                                     base ? base->GetName() : "?");
                    }
                }
            }
        }
        a_effect->magnitude = newMag;

        logger::info("Scaled '{}' ({}) magnitude {} -> {}.", base ? base->GetName() : "?", static_cast<int>(archetype), oldMag, newMag);
    }

    void EaseDiseaseHandler::Revalidate() {
        const float multiplier = ComputeMultiplier();
        if (multiplier != _lastMultiplier) {
            logger::info("Ease disease level changed: x{} -> x{}.", _lastMultiplier, multiplier);
        }
        _lastMultiplier = multiplier;
        _watch.store(multiplier < 1.0f, std::memory_order_relaxed);

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* magicTarget = player ? player->GetMagicTarget() : nullptr;
        auto* effects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!effects) return;

        for (auto* effect : *effects) {
            if (IsDiseaseEffect(effect)) ScaleEffect(effect, multiplier);
        }
    }

    void EaseDiseaseHandler::QueueRevalidate() {
        if (_revalidateQueued) return;
        _revalidateQueued = true;

        SKSE::GetTaskInterface()->AddTask([this]() {
            _revalidateQueued = false;
            Revalidate();
        });
    }

    void EaseDiseaseHandler::Resync() {
        Revalidate();
        logger::info("Resync: ease disease level x{}.", _lastMultiplier);
    }

    RE::BSEventNotifyControl EaseDiseaseHandler::ProcessEvent(const RE::TESActiveEffectApplyRemoveEvent* a_event, RE::BSTEventSource<RE::TESActiveEffectApplyRemoveEvent>*) {
        if (!a_event || !a_event->target || _keywords.empty()) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_event->target.get() != player) return RE::BSEventNotifyControl::kContinue;

        if (!a_event->isApplied) {
            if (_lastMultiplier < 1.0f) QueueRevalidate();
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* effect = FindActiveEffect(player, a_event->activeEffectUniqueID);
        if (!effect) {
            QueueRevalidate();
            return RE::BSEventNotifyControl::kContinue;
        }

        if (GetEaseMultiplier(effect) || IsDiseaseEffect(effect)) QueueRevalidate();

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EaseDiseaseHandler::ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) {
        if (!a_event || !a_event->target || _keywords.empty()) return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_event->target.get() != player) return RE::BSEventNotifyControl::kContinue;

        auto* mgef = RE::TESForm::LookupByID<RE::EffectSetting>(a_event->magicEffect);
        if (!mgef) return RE::BSEventNotifyControl::kContinue;

        if (HasEaseKeyword(mgef)) {
            logger::info("Ease keyword effect '{}' ({:08X}) hit the player, revalidating.", mgef->GetName(), mgef->GetFormID());
            QueueRevalidate();
        } else if (_lastMultiplier < 1.0f) {
            QueueRevalidate();
        }

        return RE::BSEventNotifyControl::kContinue;
    }
    void Install() {
        auto* handler = EaseDiseaseHandler::GetSingleton();
        handler->ResolveForms();
        handler->Register();
        handler->StartWatcher();
    }
}