#include <Windows.h>     
#include "sdk.hpp"             
#include "jax.hpp"        

extern "C" __declspec(dllexport) int SDKVersion = SDK_VERSION;

extern "C" __declspec(dllexport) bool PluginLoad(core_sdk* sdk, void*   )
{
    g_sdk = sdk;      

    if (!sdk_init::orbwalker() || !sdk_init::target_selector() || !sdk_init::damage()  )     
    {
        g_sdk->log_console("[!] Failed to initialize required SDK modules!");
        return false;     
    }

    vengaboys::load();            

    g_sdk->log_console("[+] Jax Module Loaded!");    
    return true;     
}

extern "C" __declspec(dllexport) void PluginUnload()
{
    vengaboys::unload();            
    g_sdk->log_console("[+] Jax Module Unloaded!");
}