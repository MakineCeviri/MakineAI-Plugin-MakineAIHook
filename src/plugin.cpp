#include "makineai/plugin/plugin_api.h"
#include "hooking/hook_manager.h"
#include "settings.h"
#include "makineai/asset_parser.hpp"
#include "makineai/parsers_factory.hpp"
#include "makineai/memory_extractor.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <string>
#include <vector>

static texthook::HookManager g_hookMgr;
static texthook::Settings g_settings;
static std::string g_lastError;
static std::string g_textBuffer;  // Holds result for makineai_get_hooked_text
static bool g_ready = false;

// Asset parsing state
static makineai::parsers::AssetParser g_assetParser;
static std::vector<makineai::parsers::StringEntry> g_parsedStrings;
static std::string g_jsonBuffer;
static std::string g_engineName;

// Memory scanning state
static std::vector<makineai::TranslationEntry> g_scannedTexts;

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
        .id = "com.makineceviri.makineaihook",
        .name = "MakineAI Hook",
        .version = "0.2.0",
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

    // Register asset parsers
    g_assetParser.registerParser(makineai::createUnityBundleParser());
    g_assetParser.registerParser(makineai::createUnrealPakParser());
    g_assetParser.registerParser(makineai::createBethesdaBa2Parser());
    g_assetParser.registerParser(makineai::createGameMakerDataParser());

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

// ═══════════════════════════════════════════════════════════════
// Asset Parsing Exports
// ═══════════════════════════════════════════════════════════════

extern "C" __declspec(dllexport)
const char* makineai_detect_engine(const char* gamePath)
{
    if (!gamePath) return "unknown";

    auto* parser = g_assetParser.getParserForFile(gamePath);
    if (!parser) {
        g_engineName = "unknown";
    } else {
        g_engineName = parser->name();
    }
    return g_engineName.c_str();
}

extern "C" __declspec(dllexport)
int makineai_parse_assets(const char* filePath)
{
    if (!filePath) return -1;

    g_parsedStrings.clear();

    auto* parser = g_assetParser.getParserForFile(filePath);
    if (!parser) return -1;

    auto result = parser->parse(filePath);
    if (!result.has_value() || !result->success) return -1;

    g_parsedStrings = std::move(result->strings);
    return static_cast<int>(g_parsedStrings.size());
}

extern "C" __declspec(dllexport)
int makineai_get_string_count(void)
{
    return static_cast<int>(g_parsedStrings.size());
}

extern "C" __declspec(dllexport)
const char* makineai_get_string_at(int index)
{
    if (index < 0 || index >= static_cast<int>(g_parsedStrings.size())) return "";

    const auto& s = g_parsedStrings[index];
    nlohmann::json j;
    j["key"] = s.key;
    j["original"] = s.original;
    j["context"] = s.context;
    j["offset"] = s.offset;
    j["maxLength"] = s.maxLength;
    g_jsonBuffer = j.dump();
    return g_jsonBuffer.c_str();
}

// ═══════════════════════════════════════════════════════════════
// Memory Scanner Exports
// ═══════════════════════════════════════════════════════════════

extern "C" __declspec(dllexport)
int makineai_scan_memory(DWORD pid)
{
    if (pid == 0) return -1;

    g_scannedTexts.clear();

    auto procInfo = makineai::MemoryExtractor::openProcess(pid);
    if (!procInfo.has_value()) return -1;

    makineai::MemoryExtractor extractor;

    makineai::ExtractionConfig config;
    config.min_string_length = static_cast<size_t>(
        std::atoi(g_settings.get("minTextLength", "4").c_str()));

    extractor.setConfig(config);

    auto result = extractor.extract(procInfo.value());
    g_scannedTexts = std::move(result.entries);

    return static_cast<int>(g_scannedTexts.size());
}

extern "C" __declspec(dllexport)
const char* makineai_get_scanned_text(int index)
{
    if (index < 0 || index >= static_cast<int>(g_scannedTexts.size())) return "";

    const auto& t = g_scannedTexts[index];
    nlohmann::json j;
    j["text"] = t.text;
    j["category"] = t.category;
    j["encoding"] = t.encoding;
    j["address"] = t.address;
    j["length"] = t.length;
    g_jsonBuffer = j.dump();
    return g_jsonBuffer.c_str();
}
