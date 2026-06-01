#include "core/filter_engine.h"
#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/rule_set.h"

#include <chrono>
#include <cstdio>
#include <string>

namespace {

class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}

    double seconds() const {
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

void usage(const char* program) {
    std::fprintf(stderr, "Usage: %s <log_file> <rule_file>\n", program);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    lv::MMapFile file;
    if (!file.open(argv[1])) {
        std::fprintf(stderr, "cannot open log file: %s\n", argv[1]);
        return 1;
    }

    lv::RuleSet rules;
    std::string error;
    if (!rules.load(argv[2], &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    lv::LineIndex index;
    Timer index_timer;
    if (!index.build(file)) {
        std::fprintf(stderr, "cannot index log file\n");
        return 1;
    }
    const double index_seconds = index_timer.seconds();

    lv::FilterEngine engine;
    Timer filter_timer;
    lv::FilterResult result = engine.run(index, rules);
    const double filter_seconds = filter_timer.seconds();

    const double mib = static_cast<double>(file.size()) / (1024.0 * 1024.0);
    const double index_lines_per_sec = index_seconds > 0.0
                                           ? static_cast<double>(index.line_count()) / index_seconds
                                           : 0.0;
    const double filter_lines_per_sec = filter_seconds > 0.0
                                            ? static_cast<double>(index.line_count()) / filter_seconds
                                            : 0.0;
    const double filter_mib_per_sec = filter_seconds > 0.0 ? mib / filter_seconds : 0.0;

    std::printf("file=%s\n", argv[1]);
    std::printf("rules=%s\n", argv[2]);
    std::printf("bytes=%zu\n", file.size());
    std::printf("mib=%.2f\n", mib);
    std::printf("lines=%zu\n", index.line_count());
    std::printf("rule_count=%zu\n", rules.size());
    std::printf("visible=%zu\n", result.visible_count());
    std::printf("bitmap_bytes=%zu\n", result.bitmap_bytes());
    std::printf("index_seconds=%.6f\n", index_seconds);
    std::printf("index_lines_per_sec=%.0f\n", index_lines_per_sec);
    std::printf("filter_seconds=%.6f\n", filter_seconds);
    std::printf("filter_lines_per_sec=%.0f\n", filter_lines_per_sec);
    std::printf("filter_mib_per_sec=%.2f\n", filter_mib_per_sec);
    return 0;
}
