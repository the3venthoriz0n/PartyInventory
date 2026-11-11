#include "PCH.h"

#include "Access.h"
#include "Config.h"
#include "StrLink.h"

namespace
{
    void SetupLog()
    {
        auto logDirectory = SKSE::log::log_directory();
        if (!logDirectory) {
            return;
        }

        auto logPath = *logDirectory / "PartyInventory.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto logger = std::make_shared<spdlog::logger>("PartyInventory", std::move(sink));

        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [PartyInventory] [%l] %v");
        spdlog::set_default_logger(std::move(logger));
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message || a_message->type != SKSE::MessagingInterface::kDataLoaded) {
            return;
        }

        PartyInventory::Access::Install();

        if (!PartyInventory::StrLink::Start()) {
            spdlog::error(
                "PartyInventory is not connected to STR: it needs STRPluginMessagingAPI and STRPluginMessagingBridge");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    SetupLog();
    spdlog::info("PartyInventory 0.2.1 loading");

    PartyInventory::Config::Load();
    if (PartyInventory::Config::Get().debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        spdlog::critical("Could not register SKSE messaging listener");
        return false;
    }

    return true;
}
