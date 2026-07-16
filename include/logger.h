#pragma once
#include <spdlog/sinks/basic_file_sink.h>

static void SetupLog(bool a_enabled) {
    if (!a_enabled) {
        spdlog::set_level(spdlog::level::off);
        spdlog::flush_on(spdlog::level::off);
        return;
    }

    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%#] %v");
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
#endif
    logger::info("{} v{}", pluginName, SKSE::PluginDeclaration::GetSingleton()->GetVersion());
}
