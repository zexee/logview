#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/path_util.h"
#include "core/rule_set.h"
#include "ui/app_ui.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

void usage(const char* program) {
    std::fprintf(stderr, "Usage: %s <log_file> [rule_file]\n", program);
}

// Shared entrypoint. argv is UTF-8 on every platform; on Windows wmain
// converts the wide argv to UTF-8 before calling this.
int run(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }

    const std::string log_path = lv::to_utf8(lv::expand_path(argv[1]));
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
        rules_path = lv::to_utf8(lv::expand_path(argv[2]));
        if (!rules.load(rules_path, &error)) {
            std::fprintf(stderr, "lv: %s\n", error.c_str());
            return 1;
        }
    }

    lv::ui::AppUi ui(std::move(file), std::move(index), std::move(rules),
                     argc == 3 ? rules_path : "");
    return ui.run();
}

} // namespace

#if defined(_WIN32)

int wmain(int argc, wchar_t** wargv) {
    // Console defaults to the OEM/ANSI codepage; force UTF-8 so that the
    // narrow-char ncurses/PDCursesMod path and stderr both render correctly.
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    if (argc < 1) {
        return 1;
    }

    std::vector<std::string> utf8_args;
    utf8_args.reserve(static_cast<std::size_t>(argc));
    std::vector<char*> argv_ptrs;
    argv_ptrs.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1,
                                              nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return 1;
        }
        std::string s(static_cast<std::size_t>(len - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), len,
                              nullptr, nullptr);
        utf8_args.push_back(std::move(s));
    }
    for (auto& s : utf8_args) {
        argv_ptrs.push_back(s.data());
    }
    argv_ptrs.push_back(nullptr);

    return run(argc, argv_ptrs.data());
}

#else

int main(int argc, char** argv) {
    return run(argc, argv);
}

#endif
