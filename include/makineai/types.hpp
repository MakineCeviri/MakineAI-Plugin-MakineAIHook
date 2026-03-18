#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace makineai {

namespace fs = std::filesystem;
using StringList = std::vector<std::string>;

enum class GameEngine {
    Unknown = 0,
    Unity_Mono,
    Unity_IL2CPP,
    Unreal,
    RPGMaker_MV,
    RPGMaker_MZ,
    RPGMaker_VXAce,
    RenPy,
    GameMaker,
    Bethesda,
    Godot,
    Custom,
};

struct TranslationEntry {
    uint32_t hash{0};
    uint32_t meta{0};
    std::string text;
    std::string raw_text;
    std::string category;
    std::string encoding;
    uint64_t address{0};
    uint32_t length{0};
};

} // namespace makineai
