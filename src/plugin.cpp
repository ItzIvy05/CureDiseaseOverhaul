#include "logger.h"
#include "Events.h"
#include "Settings.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Settings::LogSummary();
        Events::Install();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        Events::EaseDiseaseHandler::GetSingleton()->Resync();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    Settings::Load();
    SetupLog(Settings::g_enableLogging);
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}