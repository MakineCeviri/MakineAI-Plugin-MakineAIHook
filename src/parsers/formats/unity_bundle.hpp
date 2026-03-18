/**
 * @file unity_bundle.hpp
 * @brief Unity AssetBundle format structures
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <cstdint>
#include <string>

namespace makineai::formats {

// Unity magic strings
constexpr const char* kUnityFSMagic  = "UnityFS";
constexpr const char* kUnityWebMagic = "UnityWe";
constexpr const char* kUnityRawMagic = "UnityRa";

enum class UnityCompression : uint32_t {
    None  = 0,
    LZMA  = 1,
    LZ4   = 2,
    LZ4HC = 3,
};

struct UnityFSHeader {
    char signature[8]{};
    uint32_t formatVersion = 0;
    std::string unityVersion;
    std::string generatorVersion;
    uint64_t totalSize = 0;
    uint32_t compressedBlocksInfoSize = 0;
    uint32_t uncompressedBlocksInfoSize = 0;
    uint32_t flags = 0;

    [[nodiscard]] UnityCompression compressionType() const noexcept {
        return static_cast<UnityCompression>(flags & 0x3F);
    }
};

struct UnityStorageBlock {
    uint32_t uncompressedSize = 0;
    uint32_t compressedSize = 0;
    uint16_t flags = 0;

    [[nodiscard]] UnityCompression compressionType() const noexcept {
        return static_cast<UnityCompression>(flags & 0x3F);
    }
};

struct UnityNode {
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t flags = 0;
    std::string path;

    [[nodiscard]] bool isSerializedFile() const noexcept {
        // Flags bit 2 indicates serialized file, or check path extension
        return (flags & 0x04) != 0 ||
               path.ends_with(".assets") ||
               path.ends_with(".resource") ||
               path.ends_with(".resS");
    }
};

} // namespace makineai::formats
