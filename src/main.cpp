#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/rule_set.h"
#include "ui/app_ui.h"

#include <cstdio>
#include <string>
#include <utility>

namespace {

void usage(const char* program) {
    std::fprintf(stderr, "Usage: %s <log_file> [rule_file]\n", program);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }

    lv::MMapFile file;
    if (!file.open(argv[1])) {
        std::fprintf(stderr, "lv: cannot open log file: %s\n", argv[1]);
        return 1;
    }

    lv::LineIndex index;
    if (!index.build(file)) {
        std::fprintf(stderr, "lv: cannot index log file: %s\n", argv[1]);
        return 1;
    }

    lv::RuleSet rules;
    if (argc == 3) {
        std::string error;
        if (!rules.load(argv[2], &error)) {
            std::fprintf(stderr, "lv: %s\n", error.c_str());
            return 1;
        }
    }

    lv::ui::AppUi ui(std::move(file), std::move(index), std::move(rules), argc == 3 ? argv[2] : "");
    return ui.run();
}
