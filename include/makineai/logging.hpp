#pragma once

#include <cstdio>

namespace makineai::log {
    constexpr const char* PARSER = "parser";
    constexpr const char* MEMORY = "memory";
    constexpr const char* HOOK   = "hook";
} // namespace makineai::log

#ifdef MAKINEAI_DEBUG_LOG
  #define MAKINEAI_LOG_DEBUG(cat, fmt, ...) \
      std::fprintf(stderr, "[DEBUG][%s] " fmt "\n", cat, ##__VA_ARGS__)
  #define MAKINEAI_LOG_INFO(cat, fmt, ...) \
      std::fprintf(stderr, "[INFO][%s] " fmt "\n", cat, ##__VA_ARGS__)
  #define MAKINEAI_LOG_WARN(cat, fmt, ...) \
      std::fprintf(stderr, "[WARN][%s] " fmt "\n", cat, ##__VA_ARGS__)
  #define MAKINEAI_LOG_ERROR(cat, fmt, ...) \
      std::fprintf(stderr, "[ERROR][%s] " fmt "\n", cat, ##__VA_ARGS__)
#else
  #define MAKINEAI_LOG_DEBUG(cat, fmt, ...) ((void)0)
  #define MAKINEAI_LOG_INFO(cat, fmt, ...)  ((void)0)
  #define MAKINEAI_LOG_WARN(cat, fmt, ...)  ((void)0)
  #define MAKINEAI_LOG_ERROR(cat, fmt, ...) ((void)0)
#endif
