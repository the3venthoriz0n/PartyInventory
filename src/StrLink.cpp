#include "PCH.h"
#include "StrLink.h"

#include "Config.h"
#include "Util.h"

namespace PartyInventory::StrLink
{
    namespace
    {
        constexpr const char*      kChannel = "partyinventory.v1";
        constexpr std::string_view kPrefix = "PIv1|";
        constexpr auto             kAnnounceInterval = std::chrono::seconds(15);
        // A peer's consent counts only if it was refreshed recently, so a stale "allow" cannot linger.
        constexpr auto kConsentTtl = std::chrono::seconds(45);

        struct PeerState
        {
            bool                                  allow{ false };
            std::string                           name;
            std::chrono::steady_clock::time_point lastSeen{};
        };

        const STRPM::Interface*              g_api{ nullptr };
        const STRPM::ProxyResolverInterface* g_resolver{ nullptr };
        STRPM::ListenerHandle                g_channelListener{};
        std::atomic<bool>                    g_running{ false };
        std::atomic<bool>                    g_announceSoon{ false };

        std::mutex                                          g_mutex;
        std::unordered_map<RE::FormID, STRPM::ConnectionID> g_proxies;
        std::unordered_map<STRPM::ConnectionID, PeerState>  g_peers;

        std::jthread g_announcer;

        bool SendText(const STRPM::Target& a_target, std::string_view a_text)
        {
            if (!g_running.load() || !g_api || !g_api->send) {
                return false;
            }

            std::string payload(kPrefix);
            payload += a_text;

            constexpr auto flags = STRPM::kMessageReliable | STRPM::kMessageOrdered;
            const auto     result = g_api->send(kChannel, a_target, payload.data(), payload.size(), flags);
            if (result != STRPM::Result::kOk) {
                // Not connected / no peers is normal outside a session.
                spdlog::debug("STRPM send '{}' failed: {}", a_text, STRPM::ResultToString(result));
                return false;
            }
            return true;
        }

        std::string DisplayName(STRPM::ConnectionID a_connection, std::string_view a_fallback)
        {
            std::scoped_lock lock(g_mutex);
            if (const auto it = g_peers.find(a_connection); it != g_peers.end() && !it->second.name.empty()) {
                return it->second.name;
            }
            return std::string(a_fallback);
        }

        void STRPM_CALL OnProxyMapping(const STRPM::ProxyMappingEvent* a_event, void*)
        {
            if (!a_event) {
                return;
            }

            std::scoped_lock lock(g_mutex);
            if (a_event->type == STRPM::ProxyMappingEventType::kCleared) {
                g_proxies.clear();
                spdlog::info("Proxy mappings cleared");
                return;
            }

            if (a_event->oldFormID != STRPM::kInvalidProxyFormID) {
                g_proxies.erase(a_event->oldFormID);
            }

            if (a_event->newFormID != STRPM::kInvalidProxyFormID && a_event->connectionID != 0) {
                g_proxies[a_event->newFormID] = a_event->connectionID;
                // A peer just appeared or changed proxy: announce our state again soon.
                g_announceSoon.store(true);
                spdlog::info("Proxy mapped: connection={} form={:08X}", a_event->connectionID, a_event->newFormID);
            } else if (a_event->type == STRPM::ProxyMappingEventType::kRemoved) {
                spdlog::info("Proxy removed: connection={} form={:08X}", a_event->connectionID, a_event->oldFormID);
            }
        }

        void STRPM_CALL OnMessage(const STRPM::Message* a_message, void*)
        {
            // Runs on the bridge's dispatcher thread, not the game thread: keep it small.
            if (!a_message || !a_message->channel || !a_message->data || a_message->size == 0 || a_message->size > 256) {
                return;
            }
            if (std::string_view(a_message->channel) != kChannel) {
                return;
            }

            std::string_view text(static_cast<const char*>(a_message->data), a_message->size);
            if (!text.starts_with(kPrefix)) {
                return;
            }
            text.remove_prefix(kPrefix.size());

            const auto separator = text.find('|');
            if (separator == std::string_view::npos) {
                return;
            }
            const auto type = text.substr(0, separator);
            const auto argument = text.substr(separator + 1);

            const auto  connection = a_message->sender.connectionID;
            std::string senderName = a_message->sender.displayName ? a_message->sender.displayName : "";

            if (type == "ACCESS") {
                std::scoped_lock lock(g_mutex);
                auto&            peer = g_peers[connection];
                const bool       allow = argument == "1";
                if (peer.allow != allow) {
                    spdlog::info("Peer {} ('{}') access state: {}", connection, senderName, allow ? "allowed" : "not allowed");
                }
                peer.allow = allow;
                peer.lastSeen = std::chrono::steady_clock::now();
                if (!senderName.empty()) {
                    peer.name = senderName;
                }
            } else if (type == "OPEN") {
                if (!senderName.empty()) {
                    std::scoped_lock lock(g_mutex);
                    g_peers[connection].name = senderName;
                }
                const auto name = DisplayName(connection, "A party member");
                const bool open = argument == "1";
                spdlog::info("{} {} your inventory", name, open ? "opened" : "closed");
                ShowHud(open ? name + " is looking through your inventory." : name + " closed your inventory.");
            }
        }
    }

    bool Start()
    {
        if (g_running.load()) {
            return true;
        }

        g_api = STRPM::LoadFromModule();
        if (!g_api) {
            spdlog::error("STRPluginMessagingAPI.dll not found or incompatible; PartyInventory cannot tell who the players are");
            return false;
        }

        g_resolver = STRPM::LoadProxyResolverFromModule();
        if (!g_resolver || !g_resolver->registerListener || !g_resolver->unregisterListener) {
            spdlog::error("STRPM proxy resolver unavailable: install a current STRPluginMessagingAPI build");
            g_api = nullptr;
            g_resolver = nullptr;
            return false;
        }

        {
            std::scoped_lock lock(g_mutex);
            g_proxies.clear();
            g_peers.clear();
        }

        STRPM::ListenerHandle listener{};
        if (const auto result = g_api->registerChannel(kChannel, &OnMessage, nullptr, &listener);
            result != STRPM::Result::kOk) {
            spdlog::error("STRPM registerChannel failed: {}", STRPM::ResultToString(result));
            g_api = nullptr;
            g_resolver = nullptr;
            return false;
        }

        if (const auto result = g_resolver->registerListener(&OnProxyMapping, nullptr);
            result != STRPM::Result::kOk) {
            spdlog::error("STRPM proxy listener registration failed: {}", STRPM::ResultToString(result));
            g_api->unregisterChannel(listener);
            g_api = nullptr;
            g_resolver = nullptr;
            return false;
        }

        g_channelListener = listener;
        g_running.store(true);
        g_announceSoon.store(true);

        g_announcer = std::jthread([](std::stop_token a_stop) {
            auto last = std::chrono::steady_clock::now();
            auto lastReload = last;
            bool lastAllow = Config::AllowOthers();
            while (!a_stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                const auto now = std::chrono::steady_clock::now();

                // Pick up changes made in the MCM (or by hand) about once a second.
                if (now - lastReload >= std::chrono::seconds(1)) {
                    Config::ReloadIfChanged();
                    lastReload = now;
                }

                // Tell everyone right away when "allow others" changes.
                const bool allow = Config::AllowOthers();
                if (allow != lastAllow) {
                    lastAllow = allow;
                    g_announceSoon.store(true);
                }

                if (g_announceSoon.exchange(false) || now - last >= kAnnounceInterval) {
                    BroadcastAccessState();
                    last = now;
                }
            }
        });

        spdlog::info("STRPM link started: channel={}", kChannel);
        return true;
    }

    void Stop()
    {
        if (!g_running.exchange(false)) {
            return;
        }

        if (g_announcer.joinable()) {
            g_announcer.request_stop();
            g_announcer.join();
        }
        if (g_resolver && g_resolver->unregisterListener) {
            g_resolver->unregisterListener(&OnProxyMapping, nullptr);
        }
        if (g_api && g_api->unregisterChannel) {
            g_api->unregisterChannel(g_channelListener);
        }
        g_api = nullptr;
        g_resolver = nullptr;
    }

    bool IsRunning() noexcept
    {
        return g_running.load();
    }

    std::optional<STRPM::ConnectionID> ResolveProxy(RE::FormID a_actorFormID)
    {
        std::scoped_lock lock(g_mutex);
        if (const auto it = g_proxies.find(a_actorFormID); it != g_proxies.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool PeerAllowsAccess(STRPM::ConnectionID a_connection)
    {
        std::scoped_lock lock(g_mutex);
        const auto       it = g_peers.find(a_connection);
        if (it == g_peers.end() || !it->second.allow) {
            return false;
        }
        return std::chrono::steady_clock::now() - it->second.lastSeen <= kConsentTtl;
    }

    void BroadcastAccessState()
    {
        const STRPM::Target everyone{ STRPM::TargetKind::kAllPlayers, 0, nullptr };
        SendText(everyone, Config::AllowOthers() ? "ACCESS|1" : "ACCESS|0");
    }

    void SendOpenState(STRPM::ConnectionID a_connection, bool a_open)
    {
        const STRPM::Target player{ STRPM::TargetKind::kPlayer, a_connection, nullptr };
        SendText(player, a_open ? "OPEN|1" : "OPEN|0");
    }
}
