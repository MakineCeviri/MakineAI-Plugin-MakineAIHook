/**
 * @file unreal_pak.hpp
 * @brief Unreal Engine PAK format structures
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <cstdint>

namespace makineai::formats {

// PAK footer magic
constexpr uint32_t kUnrealPakMagic = 0x5A6F12E1;

// LocRes magic
constexpr uint32_t kLocResMagic = 0x0E14DAD9;

struct PakFooter {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t indexOffset = 0;
    uint64_t indexSize = 0;
    uint8_t hash[20]{};
};

} // namespace makineai::formats
