#include "PCH.h"
#include "Util.h"

#include "Config.h"

namespace PartyInventory
{
    void ShowHud(std::string a_message)
    {
        if (!Config::Get().notifications) {
            return;
        }

        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            return;
        }

        tasks->AddTask([message = std::move(a_message)]() {
            RE::SendHUDMessage::ShowHUDMessage(message.c_str());
        });
    }

    std::string ActorName(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return {};
        }

        const char* name = a_actor->GetDisplayFullName();
        if (!name || !*name) {
            name = a_actor->GetName();
        }
        return name && *name ? std::string(name) : std::string{};
    }
}
