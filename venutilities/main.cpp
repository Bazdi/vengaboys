#include <Windows.h>
#include "sdk.hpp"
#include "FiddleTracker.hpp"
#include "CassUltTurn.hpp"
#include "TryndaWPreventSlow.hpp"
#include "BuffHashLogger.hpp"
#include "SmiteUtility.hpp"

using namespace venutilities;

menu_category* main_menu_category = nullptr;
venutilities::smite_utility::SmiteUtility g_smite_utility; // Renamed to g_smite_utility to avoid namespace ambiguity

bool initialize_core_dependencies()
{
    if (!sdk_init::infotab())
    {
        g_sdk->log_console("[!] VEN Utilities: Failed to initialize menu system");
        return false;
    }

    if (!sdk_init::target_selector() || !sdk_init::orbwalker()) {
        g_sdk->log_console("[!] VEN Utilities: Failed to initialize target selector or orbwalker");
        return false;
    }

    if (!fiddle_tracker::initialize_dependencies()) {
        return false;
    }
    //if (!cass_ult_turn::initialize_dependencies()) {
    //    return false;
    //}
    //if (!trynda_w_prevent_slow::initialize_dependencies()) {
    //    return false;
    //}

    return true;
}

void setup_main_menu()
{
    main_menu_category = g_sdk->menu_manager->add_category("venutilities", "VEN Utilities");
    if (!main_menu_category) {
        g_sdk->log_console("[!] VEN Utilities: Failed to create main menu category");
        return;
    }

    fiddle_tracker::setup_menu(main_menu_category);
    //cass_ult_turn::setup_menu(main_menu_category);
    //trynda_w_prevent_slow::setup_menu(main_menu_category);
    //g_smite_utility.initialize(main_menu_category); // Use the renamed global instance
}


extern "C" __declspec(dllexport) int SDKVersion = SDK_VERSION;

extern "C" __declspec(dllexport) bool PluginLoad(core_sdk* sdk, void** custom_sdk) {
    g_sdk = sdk;

    g_sdk->log_console("[+] Loading VEN Utilities...");

    if (!initialize_core_dependencies())
    {
        g_sdk->log_console("[!] VEN Utilities: Core dependency initialization failed!");
        return false;
    }

    setup_main_menu();
    //buff_hash_logger::load();
    fiddle_tracker::load(main_menu_category);
    //cass_ult_turn::load(main_menu_category);
    //trynda_w_prevent_slow::load(main_menu_category);

    g_sdk->log_console("[+] VEN Utilities loaded successfully!");
    return true;
}


extern "C" __declspec(dllexport) void PluginUnload() {
    g_sdk->log_console("[-] Unloading VEN Utilities...");

    //buff_hash_logger::unload();
    fiddle_tracker::unload();
    //cass_ult_turn::unload();
    //trynda_w_prevent_slow::unload();
    //g_smite_utility.cleanup(); // Use the renamed global instance

    g_sdk->log_console("[-] VEN Utilities unloaded!");
}