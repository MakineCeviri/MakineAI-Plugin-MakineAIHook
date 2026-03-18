/**
 * @file bethesda_ba2.hpp
 * @brief Bethesda BA2 archive format structures
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <cstdint>

namespace makineai::formats {

// BA2 magic: "BTDX"
constexpr uint32_t kBa2Magic = 0x58445442;

struct Ba2Header {
    uint32_t magic = 0;      // "BTDX"
    uint32_t version = 0;    // 1 = FO4, 2 = SF
    uint32_t type = 0;       // GNRL, DX10, GNMF
    uint32_t fileCount = 0;
    uint64_t nameTableOffset = 0;
};

struct Ba2FileEntry {
    uint32_t nameHash = 0;
    char extension[4]{};
    uint32_t dirHash = 0;
    uint32_t flags = 0;
    uint64_t offset = 0;
    uint32_t packedSize = 0;
    uint32_t unpackedSize = 0;
    uint32_t align = 0;
};

} // namespace makineai::formats
