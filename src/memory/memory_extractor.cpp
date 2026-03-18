/**
 * @file memory_extractor.cpp
 * @brief MemoryExtractor implementation — Windows process memory scanning
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/memory_extractor.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace makineai {

// ────────────────────────────────────────────────────────────────
// Turkish fingerprint utilities
// ────────────────────────────────────────────────────────────────

namespace turkish {

bool containsTurkishChars(std::string_view text) {
    // Scan for 2-byte UTF-8 sequences of Turkish special chars
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        auto b0 = static_cast<uint8_t>(text[i]);
        auto b1 = static_cast<uint8_t>(text[i + 1]);

        // İ(C4 B0), ı(C4 B1), Ğ(C4 9E), ğ(C4 9F)
        if (b0 == 0xC4 && (b1 == 0xB0 || b1 == 0xB1 || b1 == 0x9E || b1 == 0x9F))
            return true;
        // Ş(C5 9E), ş(C5 9F)
        if (b0 == 0xC5 && (b1 == 0x9E || b1 == 0x9F))
            return true;
        // Ç(C3 87), ç(C3 A7), Ö(C3 96), ö(C3 B6), Ü(C3 9C), ü(C3 BC)
        if (b0 == 0xC3 && (b1 == 0x87 || b1 == 0xA7 || b1 == 0x96 || b1 == 0xB6 ||
                           b1 == 0x9C || b1 == 0xBC))
            return true;
    }
    return false;
}

bool containsTurkishBytes(std::span<const uint8_t> data) {
    return containsTurkishChars(
        std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

float turkishScore(std::string_view text) {
    if (text.empty()) return 0.0f;
    int turkish_chars = 0;
    int total_alpha = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        auto b0 = static_cast<uint8_t>(text[i]);
        if (b0 < 0x80) {
            if ((b0 >= 'a' && b0 <= 'z') || (b0 >= 'A' && b0 <= 'Z'))
                ++total_alpha;
        } else if (i + 1 < text.size()) {
            auto b1 = static_cast<uint8_t>(text[i + 1]);
            bool is_turkish = false;
            if (b0 == 0xC4 && (b1 == 0xB0 || b1 == 0xB1 || b1 == 0x9E || b1 == 0x9F))
                is_turkish = true;
            if (b0 == 0xC5 && (b1 == 0x9E || b1 == 0x9F))
                is_turkish = true;
            if (b0 == 0xC3 && (b1 == 0x87 || b1 == 0xA7 || b1 == 0x96 || b1 == 0xB6 ||
                               b1 == 0x9C || b1 == 0xBC))
                is_turkish = true;
            if (is_turkish) {
                ++turkish_chars;
                ++total_alpha;
                ++i; // skip second byte
            }
        }
    }

    if (total_alpha == 0) return 0.0f;
    return static_cast<float>(turkish_chars) / static_cast<float>(total_alpha);
}

} // namespace turkish

// ────────────────────────────────────────────────────────────────
// Encoding obfuscation
// ────────────────────────────────────────────────────────────────

EncodingFixMap detectEncodingObfuscation(const std::vector<std::string>& /*sample_texts*/) {
    // Stub — encoding obfuscation detection is engine-specific
    return {};
}

std::string applyEncodingFix(std::string_view text, const EncodingFixMap& /*fixes*/) {
    return std::string(text);
}

// ────────────────────────────────────────────────────────────────
// MemoryExtractionResult
// ────────────────────────────────────────────────────────────────

std::unordered_map<uint32_t, std::string> MemoryExtractionResult::hashMap() const {
    std::unordered_map<uint32_t, std::string> map;
    for (const auto& e : entries) {
        if (e.hash != 0) {
            map[e.hash] = e.text;
        }
    }
    return map;
}

// ────────────────────────────────────────────────────────────────
// Engine modules — stub implementations
// ────────────────────────────────────────────────────────────────

bool RAGEEngineModule::detect(std::span<const uint8_t> data) const {
    // Look for GXT2 magic "2TXG" or "GXT2"
    if (data.size() < 4) return false;
    const char* p = reinterpret_cast<const char*>(data.data());
    for (size_t i = 0; i + 3 < data.size(); ++i) {
        if (std::memcmp(p + i, "GXT2", 4) == 0 || std::memcmp(p + i, "2TXG", 4) == 0)
            return true;
    }
    return false;
}

std::vector<TranslationEntry> RAGEEngineModule::extractFromRegion(
    std::span<const uint8_t> /*data*/, uint64_t /*base_addr*/) const {
    // Full RAGE extraction requires GXT2 table parsing — deferred
    return {};
}

bool UnrealEngineModule::detect(std::span<const uint8_t> data) const {
    // Look for LocRes magic bytes
    if (data.size() < 16) return false;
    const char* p = reinterpret_cast<const char*>(data.data());
    for (size_t i = 0; i + 5 < data.size(); ++i) {
        if (std::memcmp(p + i, "LocRe", 5) == 0)
            return true;
    }
    return false;
}

std::vector<TranslationEntry> UnrealEngineModule::extractFromRegion(
    std::span<const uint8_t> /*data*/, uint64_t /*base_addr*/) const {
    return {};
}

bool UnityEngineModule::detect(std::span<const uint8_t> data) const {
    // Look for I2 Localization or Unity serialization markers
    if (data.size() < 8) return false;
    const char* p = reinterpret_cast<const char*>(data.data());
    for (size_t i = 0; i + 3 < data.size(); ++i) {
        if (std::memcmp(p + i, "I2Lo", 4) == 0)
            return true;
    }
    return false;
}

std::vector<TranslationEntry> UnityEngineModule::extractFromRegion(
    std::span<const uint8_t> /*data*/, uint64_t /*base_addr*/) const {
    return {};
}

bool GenericEngineModule::detect(std::span<const uint8_t> /*data*/) const {
    // Generic always matches as fallback
    return true;
}

std::vector<TranslationEntry> GenericEngineModule::extractFromRegion(
    std::span<const uint8_t> data, uint64_t base_addr) const {
    // Scan for UTF-8 strings containing Turkish characters
    std::vector<TranslationEntry> entries;
    if (data.size() < 4) return entries;

    const char* p = reinterpret_cast<const char*>(data.data());
    size_t i = 0;

    while (i < data.size()) {
        // Find start of a printable string
        if (data[i] < 0x20 || data[i] == 0x7F) { ++i; continue; }

        // Measure string length
        size_t start = i;
        while (i < data.size() && data[i] >= 0x20 && data[i] != 0x7F) ++i;
        size_t len = i - start;

        if (len < 4 || len > 4096) continue;

        std::string_view sv(p + start, len);
        if (!turkish::containsTurkishChars(sv)) continue;

        TranslationEntry entry;
        entry.text = std::string(sv);
        entry.address = base_addr + start;
        entry.length = static_cast<uint32_t>(len);
        entry.category = "general";
        entry.encoding = "utf-8";
        entries.push_back(std::move(entry));
    }

    return entries;
}

// ────────────────────────────────────────────────────────────────
// MemoryExtractor::Impl
// ────────────────────────────────────────────────────────────────

struct MemoryExtractor::Impl {
    ExtractionConfig config;
    std::vector<std::unique_ptr<IEngineModule>> modules;
    MemoryExtractor::ProgressCallback progress_cb;

    std::vector<MemoryRegion> enumerateRegions(HANDLE hProcess) {
        std::vector<MemoryRegion> regions;
        MEMORY_BASIC_INFORMATION mbi{};
        uint8_t* addr = nullptr;

        while (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi))) {
            // Only scan committed, readable memory
            if (mbi.State == MEM_COMMIT &&
                (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
                if (mbi.RegionSize <= config.max_region_size) {
                    regions.push_back({
                        reinterpret_cast<uint64_t>(mbi.BaseAddress),
                        mbi.RegionSize,
                        mbi.Protect
                    });
                }
            }
            addr = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            if (reinterpret_cast<uint64_t>(addr) < reinterpret_cast<uint64_t>(mbi.BaseAddress))
                break; // overflow
        }
        return regions;
    }

    IEngineModule* detectEngine(HANDLE hProcess, const std::vector<MemoryRegion>& regions) {
        // Read a sample from first few regions and try to detect engine
        std::vector<uint8_t> sample;
        size_t sampled = 0;
        constexpr size_t SAMPLE_LIMIT = 4 * 1024 * 1024; // 4MB

        for (const auto& region : regions) {
            if (sampled >= SAMPLE_LIMIT) break;
            size_t toRead = std::min(region.size, SAMPLE_LIMIT - sampled);
            size_t offset = sample.size();
            sample.resize(offset + toRead);

            SIZE_T bytesRead = 0;
            if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(region.base),
                                   sample.data() + offset, toRead, &bytesRead)) {
                sample.resize(offset);
                continue;
            }
            sample.resize(offset + bytesRead);
            sampled += bytesRead;
        }

        std::span<const uint8_t> span(sample);
        for (auto& mod : modules) {
            if (mod->name() != "generic" && mod->detect(span)) {
                return mod.get();
            }
        }

        // Fall back to generic
        for (auto& mod : modules) {
            if (mod->name() == "generic") return mod.get();
        }
        return nullptr;
    }
};

// ────────────────────────────────────────────────────────────────
// MemoryExtractor public API
// ────────────────────────────────────────────────────────────────

MemoryExtractor::MemoryExtractor() : m_impl(std::make_unique<Impl>()) {
    // Register built-in modules
    m_impl->modules.push_back(std::make_unique<RAGEEngineModule>());
    m_impl->modules.push_back(std::make_unique<UnrealEngineModule>());
    m_impl->modules.push_back(std::make_unique<UnityEngineModule>());
    m_impl->modules.push_back(std::make_unique<GenericEngineModule>());
}

MemoryExtractor::~MemoryExtractor() = default;
MemoryExtractor::MemoryExtractor(MemoryExtractor&&) noexcept = default;
MemoryExtractor& MemoryExtractor::operator=(MemoryExtractor&&) noexcept = default;

void MemoryExtractor::registerModule(std::unique_ptr<IEngineModule> module) {
    if (module) {
        m_impl->modules.push_back(std::move(module));
    }
}

void MemoryExtractor::setConfig(const ExtractionConfig& config) {
    m_impl->config = config;
}

void MemoryExtractor::setProgressCallback(ProgressCallback cb) {
    m_impl->progress_cb = std::move(cb);
}

std::optional<ProcessInfo> MemoryExtractor::findProcess(std::string_view exe_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return std::nullopt;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!Process32FirstW(snapshot, &pe)) {
        CloseHandle(snapshot);
        return std::nullopt;
    }

    do {
        // Convert wide name to narrow for comparison
        char narrow[MAX_PATH]{};
        WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, narrow, MAX_PATH, nullptr, nullptr);

        if (exe_name == narrow) {
            CloseHandle(snapshot);
            return openProcess(pe.th32ProcessID);
        }
    } while (Process32NextW(snapshot, &pe));

    CloseHandle(snapshot);
    return std::nullopt;
}

std::optional<ProcessInfo> MemoryExtractor::openProcess(uint32_t pid) {
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) return std::nullopt;

    ProcessInfo info;
    info.pid = pid;
    info.handle = hProcess;

    // Get exe path
    char path[MAX_PATH]{};
    DWORD pathSize = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
        info.exe_path = path;
        // Extract name from path
        auto pos = info.exe_path.find_last_of("\\/");
        info.name = (pos != std::string::npos)
            ? info.exe_path.substr(pos + 1)
            : info.exe_path;
    }

    return info;
}

MemoryExtractionResult MemoryExtractor::extract(
    std::string_view process_name,
    std::string_view engine_hint)
{
    auto proc = findProcess(process_name);
    if (!proc) return {};
    auto result = extract(proc.value(), engine_hint);
    CloseHandle(proc->handle);
    return result;
}

MemoryExtractionResult MemoryExtractor::extract(
    const ProcessInfo& process,
    std::string_view engine_hint)
{
    MemoryExtractionResult result;
    result.game_name = process.name;

    auto start = std::chrono::steady_clock::now();

    HANDLE hProcess = static_cast<HANDLE>(process.handle);
    if (!hProcess) return result;

    // Enumerate readable memory regions
    auto regions = m_impl->enumerateRegions(hProcess);
    result.stats.total_regions = regions.size();

    // Detect or select engine module
    IEngineModule* engine = nullptr;
    if (engine_hint == "auto" || engine_hint.empty()) {
        engine = m_impl->detectEngine(hProcess, regions);
    } else {
        for (auto& mod : m_impl->modules) {
            if (mod->name() == engine_hint) {
                engine = mod.get();
                break;
            }
        }
    }

    if (engine) {
        result.engine = engine->name();
    }

    // Scan each region
    std::vector<uint8_t> buffer;
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];
        result.stats.total_bytes += region.size;

        buffer.resize(region.size);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(region.base),
                               buffer.data(), region.size, &bytesRead)) {
            continue;
        }

        std::span<const uint8_t> span(buffer.data(), bytesRead);

        std::vector<TranslationEntry> entries;
        if (engine) {
            entries = engine->extractFromRegion(span, region.base);
        }

        for (auto& e : entries) {
            result.stats.raw_entries++;
            result.entries.push_back(std::move(e));
        }

        if (m_impl->progress_cb) {
            m_impl->progress_cb(i + 1, regions.size(), result.entries.size());
        }
    }

    // Deduplicate
    std::sort(result.entries.begin(), result.entries.end(),
              [](const auto& a, const auto& b) { return a.text < b.text; });
    auto last = std::unique(result.entries.begin(), result.entries.end(),
                            [](const auto& a, const auto& b) { return a.text == b.text; });
    result.entries.erase(last, result.entries.end());

    result.stats.unique_entries = result.entries.size();

    // Categorize
    for (const auto& e : result.entries) {
        if (e.category == "dialogue") result.stats.dialogue_count++;
        else if (e.category == "ui") result.stats.ui_count++;
        else result.stats.general_count++;
    }

    auto end = std::chrono::steady_clock::now();
    result.stats.scan_duration_s =
        std::chrono::duration<double>(end - start).count();

    return result;
}

bool MemoryExtractor::saveToJson(const MemoryExtractionResult& result,
                                  const std::string& path) {
    std::ofstream ofs(path);
    if (!ofs) return false;

    // Write a simple JSON format
    ofs << "{\n";
    ofs << "  \"game\": \"" << result.game_name << "\",\n";
    ofs << "  \"engine\": \"" << result.engine << "\",\n";
    ofs << "  \"count\": " << result.entries.size() << ",\n";
    ofs << "  \"entries\": [\n";
    for (size_t i = 0; i < result.entries.size(); ++i) {
        const auto& e = result.entries[i];
        ofs << "    {\"text\": \"";
        // Escape JSON string
        for (char c : e.text) {
            if (c == '"') ofs << "\\\"";
            else if (c == '\\') ofs << "\\\\";
            else if (c == '\n') ofs << "\\n";
            else if (c == '\r') ofs << "\\r";
            else if (c == '\t') ofs << "\\t";
            else ofs << c;
        }
        ofs << "\", \"address\": " << e.address
            << ", \"category\": \"" << e.category << "\"}";
        if (i + 1 < result.entries.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ]\n}\n";
    return ofs.good();
}

std::optional<MemoryExtractionResult> MemoryExtractor::loadFromJson(
    const std::string& /*path*/) {
    // Deferred — not needed for plugin C ABI
    return std::nullopt;
}

} // namespace makineai
