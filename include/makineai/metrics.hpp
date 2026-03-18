#pragma once

#include <string>

namespace makineai {

class Metrics {
public:
    static Metrics& instance() { static Metrics m; return m; }

    struct Timer { ~Timer() = default; };
    Timer timer(const char*) { return {}; }
    void increment(const char*) {}
};

} // namespace makineai
