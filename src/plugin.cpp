#include "makineai/plugin/plugin_api.h"
#include "hooking/hook_manager.h"
#include "settings.h"

#include <string>

static texthook::HookManager g_hookMgr;
static texthook::Settings g_settings;
static std::string g_lastError;
static std::string g_textBuffer;  // Holds result for makineai_get_hooked_text
static bool g_ready = false;

static void syncSettingsToManager() {
    std::string minLen = g_settings.get("minTextLength", "2");
    g_hookMgr.setMinTextLength(std::atoi(minLen.c_str()));
    g_hookMgr.setDeduplication(g_settings.get("deduplication", "true") == "true");
    g_hookMgr.setHookFilter(g_settings.get("hookFilter", "all"));
}

// --- Standard plugin ABI ---

extern "C" __declspec(dllexport)
MakineAiPluginInfo makineai_get_info() {
    return MakineAiPluginInfo{
        .id = "com.makineceviri.texthook",
        .name = "MakineAI TextHook",
        .version = "0.1.0",
        .apiVersion = MAKINEAI_PLUGIN_API_VERSION
    };
}

extern "C" __declspec(dllexport)
MakineAiError makineai_initialize(const char* dataPath) {
    if (!dataPath) return MAKINEAI_ERR_INVALID_PARAM;

    std::string dp(dataPath);

    // Load persisted settings
    g_settings.load(dp + "/settings.txt");

    if (!g_hookMgr.init(dp)) {
        g_lastError = g_hookMgr.lastError();
        return MAKINEAI_ERR_INIT_FAILED;
    }

    syncSettingsToManager();
    g_ready = true;
    return MAKINEAI_OK;
}

extern "C" __declspec(dllexport)
void makineai_shutdown() {
    g_hookMgr.shutdown();
    g_settings.save();
    g_ready = false;
}

extern "C" __declspec(dllexport)
bool makineai_is_ready() {
    return g_ready;
}

extern "C" __declspec(dllexport)
const char* makineai_get_last_error() {
    g_lastError = g_hookMgr.lastError();
    return g_lastError.c_str();
}

extern "C" __declspec(dllexport)
const char* makineai_get_setting(const char* key) {
    if (!key) return "";
    static std::string buf;
    buf = g_settings.get(key);
    return buf.c_str();
}

extern "C" __declspec(dllexport)
void makineai_set_setting(const char* key, const char* value) {
    if (!key || !value) return;
    g_settings.set(key, value);
    g_settings.save();
    syncSettingsToManager();
}

// --- TextHook-specific exports ---

extern "C" __declspec(dllexport)
bool makineai_inject_process(DWORD pid) {
    if (!g_ready) {
        g_lastError = "Plugin not initialized";
        return false;
    }
    if (g_settings.get("hookEnabled", "false") != "true") {
        g_lastError = "Hook is disabled in settings";
        return false;
    }
    if (pid == 0) {
        g_lastError = "Invalid PID";
        return false;
    }
    bool ok = g_hookMgr.injectIntoProcess(pid);
    if (!ok) g_lastError = g_hookMgr.lastError();
    return ok;
}

extern "C" __declspec(dllexport)
void makineai_detach_process() {
    g_hookMgr.detachFromProcess();
}

extern "C" __declspec(dllexport)
const char* makineai_get_hooked_text() {
    g_textBuffer = g_hookMgr.getLatestText();
    return g_textBuffer.c_str();
}

extern "C" __declspec(dllexport)
bool makineai_is_injected() {
    return g_hookMgr.isInjected();
}
