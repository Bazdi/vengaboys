#include <Windows.h>
#include "sdk.hpp"
#include "Jax.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include "Aphelios.hpp"
#include "jhin.hpp"
#include "riven.hpp"
#include "nocturne.hpp"
#include "karthus.hpp"
#include "XinZhao.hpp"
#include "Velkoz.hpp"
#include "Varus.hpp"
#include "Zeri.hpp"
#include "Teemo.hpp"
// Champion loader struct
struct ChampionInfo {
    void (*load)();
    void (*unload)();
    const char* name;
};

// Map of supported champions
static const std::unordered_map<std::string, ChampionInfo> SUPPORTED_CHAMPIONS = {
     //{"Jax", {vengaboys::load, vengaboys::unload, "Jax"}},
     {"Aphelios", {aphelios::load, aphelios::unload, "Aphelios"}},
     //{"Jhin", {jhin_damage_tracker::load, jhin_damage_tracker::unload, "Jhin"}},
     //{"Nocturne", {nocturne::load, nocturne::unload, "Nocturne"}},
     //{"XinZhao", {xinzhao::load, xinzhao::unload, "XinZhao"}},
     ////{"Riven", {riven::load, riven::unload, "Riven"}},
     //{"Velkoz", {velkoz::load, velkoz::unload, "Velkoz"}},
     //{"Karthus", {karthus::load, karthus::unload, "Karthus"}},
     //{"Varus", {varus::load, varus::unload, "Varus"}},
     // Add this to the SUPPORTED_CHAMPIONS map in main.cpp
    //{"Teemo", {teemo::load, teemo::unload, "Teemo"}},
     //{"Zeri", {zeri::load, zeri::unload, "Zeri"}},
     // Add new champions here
};

// Currently loaded champion info
static ChampionInfo current_champion;

// Module entry points
extern "C" __declspec(dllexport) int SDKVersion = SDK_VERSION;

extern "C" __declspec(dllexport) bool PluginLoad(core_sdk* sdk, void** custom_sdk) {
    g_sdk = sdk;

    auto player = g_sdk->object_manager->get_local_player();
    if (!player) {
        g_sdk->log_console("[!] Could not get local player");
        return false;
    }

    std::string champion_name = player->get_char_name();
    auto it = SUPPORTED_CHAMPIONS.find(champion_name);

    if (it != SUPPORTED_CHAMPIONS.end()) {
        g_sdk->log_console("[+] Detected %s, loading module...", champion_name.c_str());
        current_champion = it->second;
        g_sdk->set_package(current_champion.name);
        current_champion.load();
        return true;
    }

    g_sdk->log_console("[!] Champion %s is not supported", champion_name.c_str());
    return false;
}

extern "C" __declspec(dllexport) void PluginUnload() {
    if (current_champion.unload) {
        current_champion.unload();
    }
}