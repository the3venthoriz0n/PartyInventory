#include "PCH.h"
#include "Access.h"

#include "Config.h"
#include "StrLink.h"
#include "Util.h"

namespace PartyInventory::Access
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        constexpr auto kDebounce = std::chrono::milliseconds(400);
        // How long after pressing the key we accept a ContainerMenu opening as ours.
        constexpr auto kOpenGrace = std::chrono::seconds(3);

        Clock::time_point                  g_lastPress{};
        std::optional<STRPM::ConnectionID> g_pending;  // asked to open, menu not seen yet
        Clock::time_point                  g_pendingSince{};
        std::optional<STRPM::ConnectionID> g_active;   // peer whose inventory is open right now

        // The actor under the crosshair, or nullptr.
        RE::Actor* CrosshairActor()
        {
            auto* pick = RE::CrosshairPickData::GetSingleton();
            if (!pick) {
                return nullptr;
            }

            // CommonLibSSE-NG exposes the VR array layout when built for several runtimes, so the flat
            // (SE/AE) "targetActor" member is read by offset.
            RE::ObjectRefHandle handle = REL::RelocateMember<RE::ObjectRefHandle>(pick, 0x08, 0x10);
            auto                ref = handle.get();
            if (!ref) {
                handle = pick->GetActiveTarget();
                ref = handle.get();
            }
            return ref ? ref->As<RE::Actor>() : nullptr;
        }

        bool InputBlocked()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return true;
            }
            return ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME);
        }

        void HandleOpenKey()
        {
            const auto& cfg = Config::Get();

            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* actor = CrosshairActor();
            if (!player || !actor || actor == player) {
                ShowHud("PartyInventory: look at another player first.");
                return;
            }

            const auto formID = actor->GetFormID();
            const auto connection = StrLink::ResolveProxy(formID);
            if (!connection) {
                spdlog::info("Open refused: {:08X} ('{}') is not a mapped player proxy", formID, ActorName(actor));
                ShowHud("PartyInventory: that is not a connected player.");
                return;
            }

            const auto name = ActorName(actor);

            const float distance = player->GetPosition().GetDistance(actor->GetPosition());
            if (distance > static_cast<float>(cfg.maxDistance)) {
                ShowHud(name + " is too far away.");
                return;
            }

            if (cfg.requireConsent && !StrLink::PeerAllowsAccess(*connection)) {
                spdlog::info("Open refused: '{}' (connection {}) has not allowed access", name, *connection);
                ShowHud(fmt::format(
                    "{} has not allowed access. They need PartyInventory and must press the toggle key (0x{:02X}).",
                    name,
                    cfg.toggleKey));
                return;
            }

            g_pending = *connection;
            g_pendingSince = Clock::now();

            const auto mode = static_cast<RE::ContainerMenu::ContainerMode>(cfg.mode);
            spdlog::info(
                "Opening inventory of '{}' (form {:08X}, connection {}, distance {:.0f}, mode {})",
                name,
                formID,
                *connection,
                distance,
                cfg.mode);

            // Open from a game-thread task and look the actor up again there, rather than keeping a pointer.
            SKSE::GetTaskInterface()->AddTask([formID, mode]() {
                auto* target = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (!target) {
                    spdlog::warn("Open aborted: actor {:08X} no longer exists", formID);
                    return;
                }
                RE::ContainerMenu::OpenMenu(target, mode);
            });
        }

        void HandleToggleKey()
        {
            const bool allow = !Config::AllowOthers();
            Config::SetAllowOthers(allow);
            StrLink::BroadcastAccessState();
            spdlog::info("Access for other players is now {}", allow ? "ON" : "OFF");
            ShowHud(allow ? "PartyInventory: other players can now open your inventory."
                          : "PartyInventory: other players can no longer open your inventory.");
        }

        class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                RE::InputEvent* const*                a_events,
                RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!a_events) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const auto& cfg = Config::Get();
                for (auto* event = *a_events; event; event = event->next) {
                    if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                        continue;
                    }

                    const auto* button = event->AsButtonEvent();
                    if (!button || !button->IsDown() || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                        continue;
                    }

                    const auto key = button->GetIDCode();
                    if (key != cfg.openKey && key != cfg.toggleKey) {
                        continue;
                    }

                    const auto now = Clock::now();
                    if (InputBlocked() || now - g_lastPress < kDebounce) {
                        continue;
                    }
                    g_lastPress = now;

                    if (key == cfg.openKey) {
                        HandleOpenKey();
                    } else {
                        HandleToggleKey();
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // Tells the owner when their inventory is opened and closed.
        class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent*               a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!a_event || std::string_view(a_event->menuName.c_str()) != RE::ContainerMenu::MENU_NAME) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event->opening) {
                    if (g_pending && Clock::now() - g_pendingSince <= kOpenGrace) {
                        g_active = g_pending;
                        g_pending.reset();
                        StrLink::SendOpenState(*g_active, true);
                    }
                } else if (g_active) {
                    StrLink::SendOpenState(*g_active, false);
                    g_active.reset();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        InputSink g_inputSink;
        MenuSink  g_menuSink;
    }

    void Install()
    {
        if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
            input->AddEventSink(&g_inputSink);
        } else {
            spdlog::error("BSInputDeviceManager unavailable; keys will not work");
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
        } else {
            spdlog::error("UI unavailable; owner notifications will not work");
        }

        const auto& cfg = Config::Get();
        spdlog::info("Access installed: open=0x{:02X} toggle=0x{:02X}", cfg.openKey, cfg.toggleKey);
    }
}
