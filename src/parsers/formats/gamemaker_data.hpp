/**
 * @file gamemaker_data.hpp
 * @brief GameMaker Studio data.win format structures
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <cstdint>

namespace makineai::formats {

// IFF chunk header
struct IffChunk {
    uint32_t type = 0;
    uint32_t size = 0;
};

// GEN8 chunk (game metadata)
struct Gen8Info {
    uint8_t debugMode = 0;
    uint32_t majorVersion = 0;
    uint32_t minorVersion = 0;
    uint32_t releaseVersion = 0;
    uint32_t buildVersion = 0;
};

} // namespace makineai::formats
