#include "core/rule_set.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace lv {

namespace {

std::string ltrim(std::string text) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    return text;
}

bool parse_rule_operator(const std::string& token, RuleAction* action) {
    if (token == "s") {
        *action = RuleAction::Show;
        return true;
    }
    if (token == "h") {
        *action = RuleAction::Hide;
        return true;
    }
    return false;
}

} // namespace

bool RuleSet::load(const std::string& path, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error != nullptr) {
            *error = "cannot open rule file: " + path;
        }
        return false;
    }

    std::vector<Rule> parsed;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string clean = ltrim(line);
        if (clean.empty() || clean[0] == '#') {
            continue;
        }
        RuleParseResult result = parse_line(clean);
        if (!result.ok) {
            if (error != nullptr) {
                *error = path + ":" + std::to_string(line_number) + ": " + result.error;
            }
            return false;
        }
        parsed.push_back(std::move(result.rule));
    }

    rules_ = std::move(parsed);
    return true;
}

bool RuleSet::save(const std::string& path, std::string* error) const {
    std::ofstream out(path);
    if (!out) {
        if (error != nullptr) {
            *error = "cannot write rule file: " + path;
        }
        return false;
    }
    for (const Rule& rule : rules_) {
        out << rule.serialize() << '\n';
    }
    return true;
}

void RuleSet::add(Rule rule) {
    rules_.push_back(std::move(rule));
}

bool RuleSet::insert(std::size_t index, Rule rule) {
    if (index > rules_.size()) {
        return false;
    }
    rules_.insert(rules_.begin() + static_cast<std::ptrdiff_t>(index), std::move(rule));
    return true;
}

bool RuleSet::replace(std::size_t index, Rule rule) {
    if (index >= rules_.size()) {
        return false;
    }
    rules_[index] = std::move(rule);
    return true;
}

bool RuleSet::remove(std::size_t index) {
    if (index >= rules_.size()) {
        return false;
    }
    rules_.erase(rules_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool RuleSet::move_up(std::size_t index) {
    if (index == 0 || index >= rules_.size()) {
        return false;
    }
    std::swap(rules_[index - 1], rules_[index]);
    return true;
}

bool RuleSet::move_down(std::size_t index) {
    if (index + 1 >= rules_.size()) {
        return false;
    }
    std::swap(rules_[index], rules_[index + 1]);
    return true;
}

void RuleSet::clear() {
    rules_.clear();
}

RuleParseResult RuleSet::parse_line(const std::string& line) {
    std::istringstream input(line);
    std::string operator_token;
    input >> operator_token;

    bool enabled = true;
    if (operator_token.size() > 1 && operator_token[0] == '-') {
        enabled = false;
        operator_token = operator_token.substr(1);
    }

    RuleAction action = RuleAction::Show;
    if (!parse_rule_operator(operator_token, &action)) {
        return {.ok = false, .error = "expected rule operator 's' or 'h'"};
    }

    std::string raw;
    std::getline(input, raw);
    raw = ltrim(raw);
    if (raw.empty()) {
        return {.ok = false, .error = "missing rule pattern"};
    }

    if (raw.size() >= 2) {
        const char first = raw.front();
        const char last = raw.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            raw = raw.substr(1, raw.size() - 2);
        }
    }
    if (!raw.empty() && raw.back() == '|') {
        return {.ok = false, .error = "trailing '|' in rule pattern"};
    }
    if (!raw.empty() && raw.front() == '|') {
        return {.ok = false, .error = "leading '|' in rule pattern"};
    }

    std::vector<RuleSegment> segments;
    std::size_t pos = 0;
    while (pos < raw.size()) {
        std::size_t bar = raw.find('|', pos);
        std::string seg_text = raw.substr(pos, bar - pos);
        seg_text = ltrim(seg_text);
        if (seg_text.empty()) {
            return {.ok = false, .error = "empty segment in rule pattern"};
        }

        RuleSegment seg;
        if (seg_text.size() >= 2) {
            const char sf = seg_text.front();
            const char sl = seg_text.back();
            if ((sf == '"' && sl == '"') || (sf == '\'' && sl == '\'')) {
                seg_text = seg_text.substr(1, seg_text.size() - 2);
            }
        }
        if (seg_text.size() >= 2 && seg_text.front() == '/' && seg_text.back() == '/') {
            seg.type = RuleMatchType::Regex;
            seg.pattern = seg_text.substr(1, seg_text.size() - 2);
        } else {
            seg.type = RuleMatchType::Literal;
            seg.pattern = seg_text;
        }
        segments.push_back(std::move(seg));

        if (bar == std::string::npos) {
            break;
        }
        pos = bar + 1;
    }

    try {
        auto rule = Rule(action, std::move(segments));
        rule.set_enabled(enabled);
        return {.ok = true, .rule = std::move(rule), .error = ""};
    } catch (const std::exception& ex) {
        return {.ok = false, .error = ex.what()};
    }
}

} // namespace lv
