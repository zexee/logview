#include "core/bit_array.h"
#include "core/filter_engine.h"
#include "core/line_index.h"
#include "core/mmap_file.h"
#include "core/path_util.h"
#include "core/rule_set.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

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
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist;

    const auto base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::ostringstream name;
        name << prefix << "_" << std::hex << dist(gen);
        const auto candidate = base / name.str();
        if (!std::filesystem::exists(candidate)) {
            return lv::to_utf8(candidate);
        }
    }
    std::fprintf(stderr, "temp_path: could not allocate unique name\n");
    std::exit(1);
}

std::string write_temp_file(const std::string& content) {
    const std::string path = temp_path("lv_test");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::perror("open");
        std::exit(1);
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        std::perror("write");
        std::exit(1);
    }
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
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
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
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
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
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
}

void test_line_index_empty_file() {
    std::string path = write_temp_file("");
    lv::MMapFile file;
    CHECK(file.open(path));
    lv::LineIndex index;
    CHECK(index.build(file));
    CHECK_EQ(index.line_count(), 0);
    file.close();
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
}

void test_parse_rule() {
    lv::RuleParseResult parsed = lv::RuleSet::parse_line("s ERROR");
    CHECK(parsed.ok);
    CHECK(parsed.rule.action() == lv::RuleAction::Show);
    CHECK_EQ(parsed.rule.segments().size(), 1);
    CHECK(parsed.rule.segments()[0].type == lv::RuleMatchType::Literal);
    CHECK(parsed.rule.segments()[0].pattern == "ERROR");
    CHECK(parsed.rule.passes("one ERROR line"));
    CHECK(!parsed.rule.passes("one INFO line"));
}

void test_parse_rule_regex() {
    lv::RuleParseResult parsed = lv::RuleSet::parse_line("s /error/");
    CHECK(parsed.ok);
    CHECK_EQ(parsed.rule.segments().size(), 1);
    CHECK(parsed.rule.segments()[0].type == lv::RuleMatchType::Regex);
    CHECK(parsed.rule.segments()[0].pattern == "error");
}

void test_parse_rule_or() {
    lv::RuleParseResult parsed = lv::RuleSet::parse_line("s A|B|C");
    CHECK(parsed.ok);
    CHECK_EQ(parsed.rule.segments().size(), 3);
    CHECK(parsed.rule.segments()[0].type == lv::RuleMatchType::Literal);
    CHECK(parsed.rule.segments()[0].pattern == "A");
    CHECK(parsed.rule.segments()[1].pattern == "B");
    CHECK(parsed.rule.segments()[2].pattern == "C");
}

void test_parse_rule_mixed() {
    lv::RuleParseResult parsed = lv::RuleSet::parse_line("s /[0-9]/|A");
    CHECK(parsed.ok);
    CHECK_EQ(parsed.rule.segments().size(), 2);
    CHECK(parsed.rule.segments()[0].type == lv::RuleMatchType::Regex);
    CHECK(parsed.rule.segments()[0].pattern == "[0-9]");
    CHECK(parsed.rule.segments()[1].type == lv::RuleMatchType::Literal);
    CHECK(parsed.rule.segments()[1].pattern == "A");
}

void test_parse_invalid_rule() {
    CHECK(!lv::RuleSet::parse_line("show literal ERROR").ok);
    CHECK(!lv::RuleSet::parse_line("keep ERROR").ok);
    CHECK(!lv::RuleSet::parse_line("s").ok);
    CHECK(!lv::RuleSet::parse_line("s /[/").ok);
    CHECK(!lv::RuleSet::parse_line("s A|").ok);
}

void test_ruleset_save_load() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "ERROR"}}));
    rules.add(lv::Rule(lv::RuleAction::Hide, {{lv::RuleMatchType::Regex, "DEBUG|TRACE"}}));

    std::string path = temp_path("lv_rules");
    std::string error;
    CHECK(rules.save(path, &error));

    lv::RuleSet loaded;
    CHECK(loaded.load(path, &error));
    CHECK_EQ(loaded.size(), 2);
    CHECK(loaded[0].serialize() == "s ERROR");
    CHECK(loaded[1].serialize() == "h /DEBUG|TRACE/");
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(path), ec);
}

void test_ruleset_mutation() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "A"}}));
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "B"}}));
    CHECK(rules.insert(1, lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "X"}})));
    CHECK(rules[1].segments()[0].pattern == "X");
    CHECK(!rules.insert(99, lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "bad"}})));
    CHECK(rules.remove(1));
    CHECK(rules.move_down(0));
    CHECK(rules[0].segments()[0].pattern == "B");
    CHECK(rules.move_up(1));
    CHECK(rules[0].segments()[0].pattern == "A");
    CHECK(rules.replace(1, lv::Rule(lv::RuleAction::Hide, {{lv::RuleMatchType::Literal, "C"}})));
    CHECK(rules[1].serialize() == "h C");
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
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "ERROR"}}));
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
    rules.add(lv::Rule(lv::RuleAction::Hide, {{lv::RuleMatchType::Literal, "DEBUG"}}));
    lv::FilterResult result = run_filter("INFO ok\nDEBUG detail\nERROR bad\n", rules);
    CHECK_EQ(result.visible_count(), 2);
    CHECK(result.visible(0));
    CHECK(!result.visible(1));
    CHECK(result.visible(2));
}

void test_filter_pipeline_layers() {
    lv::RuleSet rules;
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "ERROR"}}));
    rules.add(lv::Rule(lv::RuleAction::Hide, {{lv::RuleMatchType::Literal, "FATAL"}}));
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "auth"}}));

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
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Regex, "user=[0-9]+"}}));
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
    rules.add(lv::Rule(lv::RuleAction::Show, {{lv::RuleMatchType::Literal, "ERROR"}}));
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
    test_parse_rule_regex();
    test_parse_rule_or();
    test_parse_rule_mixed();
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
