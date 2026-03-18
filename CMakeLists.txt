cmake_minimum_required(VERSION 3.25)
project(makineaihook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies (vcpkg)
find_package(ZLIB REQUIRED)
find_package(lz4 REQUIRED)
find_package(nlohmann_json REQUIRED)

# ──────────────────────────────────────────────────────────────
# Plugin DLL (loaded by launcher)
# ──────────────────────────────────────────────────────────────
add_library(makineaihook SHARED
    src/plugin.cpp
    src/asset_parser.cpp
    src/memory/memory_extractor.cpp
    src/hooking/hook_manager.cpp
    src/parsers/unity_bundle_parser.cpp
    src/parsers/unreal_pak_parser.cpp
    src/parsers/bethesda_ba2_parser.cpp
    src/parsers/gamemaker_data_parser.cpp
    src/parsers/formats/rpa_archive.cpp
    src/parsers/formats/pickle_reader.cpp
)

target_include_directories(makineaihook PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(makineaihook PRIVATE
    ZLIB::ZLIB
    lz4::lz4
    nlohmann_json::nlohmann_json
    psapi
)

set_target_properties(makineaihook PROPERTIES
    PREFIX ""
    OUTPUT_NAME "makineaihook"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
)

# ──────────────────────────────────────────────────────────────
# Hook DLL (injected into game process)
# ──────────────────────────────────────────────────────────────
add_library(makineai-hook SHARED
    hook/dllmain.cpp
    hook/text_hooks.cpp
    hook/pipe_client.cpp
)

target_include_directories(makineai-hook PRIVATE
    ${CMAKE_SOURCE_DIR}/hook
)

target_link_libraries(makineai-hook PRIVATE
    gdi32
    user32
)

set_target_properties(makineai-hook PROPERTIES
    PREFIX ""
    OUTPUT_NAME "makineai-hook"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
)

# ──────────────────────────────────────────────────────────────
# Copy manifest to output
# ──────────────────────────────────────────────────────────────
configure_file(
    ${CMAKE_SOURCE_DIR}/manifest.json
    ${CMAKE_BINARY_DIR}/release/manifest.json
    COPYONLY
)

# ──────────────────────────────────────────────────────────────
# Tests (optional)
# ──────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
