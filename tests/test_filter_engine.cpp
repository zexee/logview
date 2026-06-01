#include "core/bit_array.h"
#include "core/filter_engine.h"
#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/rule_set.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace {

int passed = 0;
int failed = 0;

#define CHECK(expr)                                                                            \
    do {                                                                                       \
        if (expr) {                                                                            \
            ++passed;                                                                          \
        } else {                                                                               \
            ++failed;                                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                       \
        }                                                                                      \
    } while (false)

#define CHECK_EQ(actual, expected)                                                             \
    do {                                                                                       \
        std::size_t a = static_cast<std::size_t>(actual);                                      \
        std::size_t e = static_cast<std::size_t>(expected);                                    \
        if (a == e) {                                                                          \
            ++passed;                                                                          \
        } else {                                                                               \
            ++failed;                                                                          \
            std::printf("FAIL %s:%d: %s == %s (%zu != %zu)\n", __FILE__, __LINE__, #actual,   \
                        #expected, a, e);                                                      \
        }                                                                                      \
    } while (false)

std::string temp_path(const char* prefix) {
    std::string pattern = std::string("/tmp/") + prefix + "_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    int fd = ::mkstemp(buffer.data());
    if (fd < 0) {
        std::perror("mkstemp");
        std::exit(1);
    }
    ::close(fd);
    return buffer.data();
}

std::string write_temp_file(const std::string& content) {
    std::string path = temp_path("lv_test");
    int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC);
    if (fd < 0) {
        std::perror("open");
        std::exit(1);
    }
    const char* data = content.data();
    std::size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t written = ::write(fd, data, remaining);
        if (written <= 0) {
            std::perror("write");
            std::exit(1);
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
    ::close(fd);
    return path;
}

lv::FilterResult run_filter(const std::string& content, const lv::RuleSet& rules) {
    std::string path = write_temp_file(content);
    lv::MMapFile file;
    CHECK(file.open(path));
    lv::LineIndex index;
    CHECK(index.build(file));
    lv::FilterEngine engine;
    lv::FilterResult result = engine.run(index, rules);
    file.close();
    ::unlink(path.c_str());
    return result;
}

void test_bit_array() {
    lv::BitArray bits(130, false);
    CHECK_EQ(bits.size(), 130);
    bits.set(0, true);
    bits.set(64, true);
    bits.set(129, true);
    CHECK(bits.get(0));
    CHECK(bits.get(64));
    CHECK(bits.get(129));
    CHECK(!bits.get(128));
    CHECK_EQ(bits.count_ones(), 3);
    bits.fill(true);
    CHECK_EQ(bits.count_ones(), 130);
    bits.fill(false);
    CHECK_EQ(bits.count_ones(), 0);
}

void test_line_index_trailing_newline() {
    std::string path = write_temp_file("a\nb\nc\n");
    lv::MMapFile file;
    CHECK(file.open(path));
    lv::LineIndex index;
    CHECK(index.build(file));
    CHECK_EQ(index.line_count(), 3);
    CHECK(index.line(0) == "a");
    CHECK(index.line(1) == "b");
    CHECK(index.line(2) == "c");
    file.close();
    ::unlink(path.c_str());
}

void test_line_index_no_trailing_newline() {
    std::string path = write_temp_file("a\nb\nc");
    lv::MMapFile file;
    CHECK(file.open(path));
    lv::LineIndex index;
    CHECK(index.build(file));
    CHECK_EQ(index.line_count(), 3);
    CHECK(index.line(2) == "c");
    file.close();
    ::unlink(path.c_str());
}

void test_line_index_empty_file() {
    std::string path = write_temp_file("");
    lv::MMapFile file;
    CHECK(file.open(path));
    lv::LineIndex index;
    CHECK(index.build(file));
    CHECK_EQ(index.line_count(), 0);
    file.close();
    ::unlink(path.c_str());
}

void test_parse_rule() {
    lv::RuleParseResult parsed = lv::RuleSet::parse_line("ss ERROR");
    CHECK(parsed.ok);
    CHECK(parsed.rule.action() == lv::RuleAction::Show);
    CHECK(parsed.rule.type() == lv::RuleMatchType::Literal);
    CHECK(parsed.rule.pattern() == "ERROR");
    CHECK(parsed.rule.passes("one ERROR line"));
    CHECK(!parsed.rule.passes("one INFO line"));
}

void test_parse_invalid_rule() {
    CHECK(!lv::RuleSet::parse_line("show literal ERROR").ok);
    CHECK(!lv::RuleSet::parse_line("keep ERROR").ok);
    CHECK(!lv::RuleSet::parse_line("ss").ok);
    CHECK(!lv::RuleSet::parse_line("s [").ok);
}

void test_ruleset_save_load() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "ERROR"));
    rules.add(lv::Rule(lv::RuleAction::Hide, lv::RuleMatchType::Regex, "DEBUG|TRACE"));

    std::string path = temp_path("lv_rules");
    std::string error;
    CHECK(rules.save(path, &error));

    lv::RuleSet loaded;
    CHECK(loaded.load(path, &error));
    CHECK_EQ(loaded.size(), 2);
    CHECK(loaded[0].serialize() == "ss ERROR");
    CHECK(loaded[1].serialize() == "h DEBUG|TRACE");
    ::unlink(path.c_str());
}

void test_ruleset_mutation() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "A"));
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "B"));
    CHECK(rules.move_down(0));
    CHECK(rules[0].pattern() == "B");
    CHECK(rules.move_up(1));
    CHECK(rules[0].pattern() == "A");
    CHECK(rules.replace(1, lv::Rule(lv::RuleAction::Hide, lv::RuleMatchType::Literal, "C")));
    CHECK(rules[1].serialize() == "hh C");
    CHECK(rules.remove(0));
    CHECK_EQ(rules.size(), 1);
}

void test_filter_no_rules_all_visible() {
    lv::RuleSet rules;
    lv::FilterResult result = run_filter("a\nb\nc\n", rules);
    CHECK_EQ(result.line_count(), 3);
    CHECK_EQ(result.rule_count(), 0);
    CHECK_EQ(result.visible_count(), 3);
    CHECK(result.visible(0));
    CHECK(result.visible(1));
    CHECK(result.visible(2));
}

void test_filter_show_literal() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "ERROR"));
    lv::FilterResult result = run_filter("INFO ok\nERROR bad\nWARN meh\nERROR worse\n", rules);
    CHECK_EQ(result.visible_count(), 2);
    CHECK(!result.visible(0));
    CHECK(result.visible(1));
    CHECK(!result.visible(2));
    CHECK(result.visible(3));
    CHECK(!result.layer(0).get(0));
    CHECK(result.layer(0).get(1));
}

void test_filter_hide_literal() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Hide, lv::RuleMatchType::Literal, "DEBUG"));
    lv::FilterResult result = run_filter("INFO ok\nDEBUG detail\nERROR bad\n", rules);
    CHECK_EQ(result.visible_count(), 2);
    CHECK(result.visible(0));
    CHECK(!result.visible(1));
    CHECK(result.visible(2));
}

void test_filter_pipeline_layers() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "ERROR"));
    rules.add(lv::Rule(lv::RuleAction::Hide, lv::RuleMatchType::Literal, "FATAL"));
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "auth"));

    lv::FilterResult result = run_filter(
        "INFO auth ok\n"
        "ERROR auth failed\n"
        "ERROR FATAL auth crash\n"
        "ERROR disk failed\n",
        rules);

    CHECK_EQ(result.visible_count(), 1);
    CHECK(!result.visible(0));
    CHECK(result.visible(1));
    CHECK(!result.visible(2));
    CHECK(!result.visible(3));

    CHECK(!result.layer(0).get(0));
    CHECK(result.layer(0).get(1));
    CHECK(result.layer(0).get(2));
    CHECK(result.layer(0).get(3));

    CHECK(!result.layer(1).get(0));
    CHECK(result.layer(1).get(1));
    CHECK(!result.layer(1).get(2));
    CHECK(result.layer(1).get(3));

    CHECK(!result.layer(2).get(0));
    CHECK(result.layer(2).get(1));
    CHECK(!result.layer(2).get(2));
    CHECK(!result.layer(2).get(3));
}

void test_filter_regex() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Regex, "user=[0-9]+"));
    lv::FilterResult result = run_filter("user=abc\nuser=42\nno user\nuser=7 ok\n", rules);
    CHECK_EQ(result.visible_count(), 2);
    CHECK(!result.visible(0));
    CHECK(result.visible(1));
    CHECK(!result.visible(2));
    CHECK(result.visible(3));
}

void test_long_line() {
    std::string line(10000, 'x');
    line += "ERROR";
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, lv::RuleMatchType::Literal, "ERROR"));
    lv::FilterResult result = run_filter(line, rules);
    CHECK_EQ(result.line_count(), 1);
    CHECK_EQ(result.visible_count(), 1);
}

} // namespace

int main() {
    test_bit_array();
    test_line_index_trailing_newline();
    test_line_index_no_trailing_newline();
    test_line_index_empty_file();
    test_parse_rule();
    test_parse_invalid_rule();
    test_ruleset_save_load();
    test_ruleset_mutation();
    test_filter_no_rules_all_visible();
    test_filter_show_literal();
    test_filter_hide_literal();
    test_filter_pipeline_layers();
    test_filter_regex();
    test_long_line();

    std::printf("passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
