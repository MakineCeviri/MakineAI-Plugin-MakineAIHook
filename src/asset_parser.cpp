/**
 * @file asset_parser.cpp
 * @brief AssetParser dispatcher implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"

namespace makineai::parsers {

AssetParser::AssetParser() = default;
AssetParser::~AssetParser() = default;

void AssetParser::registerParser(std::unique_ptr<IAssetFormatParser> parser) {
    if (parser) {
        parsers_.push_back(std::move(parser));
    }
}

IAssetFormatParser* AssetParser::getParserForFile(const fs::path& file) const {
    for (const auto& parser : parsers_) {
        if (parser->canParse(file)) {
            return parser.get();
        }
    }
    return nullptr;
}

Result<ParseResult> AssetParser::parseFile(const fs::path& file) const {
    auto* parser = getParserForFile(file);
    if (!parser) {
        return std::unexpected(Error{ErrorCode::NotSupported,
            "No parser found for: " + file.string()});
    }
    return parser->parse(file);
}

Result<std::vector<ParseResult>> AssetParser::parseDirectory(
    const fs::path& directory,
    bool recursive,
    ProgressCallback progress) const
{
    std::vector<ParseResult> results;
    std::vector<fs::path> files;

    auto iterator_flags = recursive
        ? fs::directory_options::follow_directory_symlink
        : fs::directory_options::none;

    std::error_code ec;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(directory, iterator_flags, ec)) {
            if (entry.is_regular_file() && getParserForFile(entry.path())) {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(directory, ec)) {
            if (entry.is_regular_file() && getParserForFile(entry.path())) {
                files.push_back(entry.path());
            }
        }
    }

    if (ec) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Directory iteration failed: " + ec.message()});
    }

    for (size_t i = 0; i < files.size(); ++i) {
        if (progress) progress(i, files.size());

        auto result = parseFile(files[i]);
        if (result.has_value()) {
            results.push_back(std::move(result.value()));
        }
    }

    if (progress) progress(files.size(), files.size());

    return results;
}

VoidResult AssetParser::writeFile(
    const fs::path& file,
    const std::vector<StringEntry>& strings) const
{
    auto* parser = getParserForFile(file);
    if (!parser) {
        return std::unexpected(Error{ErrorCode::NotSupported,
            "No parser found for: " + file.string()});
    }
    return parser->write(file, strings);
}

GameEngine AssetParser::detectEngine(const fs::path& gameDir) const {
    std::error_code ec;

    // Check for engine-specific marker files
    for (const auto& entry : fs::directory_iterator(gameDir, ec)) {
        if (!entry.is_regular_file()) continue;

        auto* parser = getParserForFile(entry.path());
        if (!parser) continue;

        auto result = parser->parse(entry.path());
        if (result.has_value() && result->detectedEngine != GameEngine::Unknown) {
            return result->detectedEngine;
        }
    }

    return GameEngine::Unknown;
}

void AssetParser::registerBuiltinParsers() {
    // Intentionally empty — parsers are registered externally via factory functions
}

} // namespace makineai::parsers
