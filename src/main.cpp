#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/rule_set.h"
#include "ui/app_ui.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace {

void usage(const char* program) {
    std::fprintf(stderr, "Usage: %s <log_file> [rule_file]\n", program);
}

std::string expand_path(const std::string& path) {
    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home != nullptr) {
            if (path.size() == 1) {
                return std::string(home);
            }
            if (path[1] == '/') {
                return std::string(home) + path.substr(1);
            }
        }
    }
    return path;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }

    std::string log_path = expand_path(argv[1]);
    lv::MMapFile file;
    if (!file.open(log_path)) {
        std::fprintf(stderr, "lv: cannot open log file: %s\n", argv[1]);
        return 1;
    }

    lv::LineIndex index;
    if (!index.build(file)) {
        std::fprintf(stderr, "lv: cannot index log file: %s\n", argv[1]);
        return 1;
    }

    lv::RuleSet rules;
    std::string rules_path;
    if (argc == 3) {
        std::string error;
        rules_path = expand_path(argv[2]);
        if (!rules.load(rules_path, &error)) {
            std::fprintf(stderr, "lv: %s\n", error.c_str());
            return 1;
        }
    }

    lv::ui::AppUi ui(std::move(file), std::move(index), std::move(rules), argc == 3 ? rules_path : "");
    return ui.run();
}
