#include "core/rule_set.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace lv {

namespace {

std::string trim(std::string text) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

bool parse_rule_operator(const std::string& token, RuleAction* action, RuleMatchType* type) {
    if (token == "s") {
        *action = RuleAction::Show;
        *type = RuleMatchType::Regex;
        return true;
    }
    if (token == "h") {
        *action = RuleAction::Hide;
        *type = RuleMatchType::Regex;
        return true;
    }
    if (token == "ss") {
        *action = RuleAction::Show;
        *type = RuleMatchType::Literal;
        return true;
    }
    if (token == "hh") {
        *action = RuleAction::Hide;
        *type = RuleMatchType::Literal;
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
        const std::string clean = trim(line);
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

    RuleAction action = RuleAction::Show;
    RuleMatchType type = RuleMatchType::Literal;
    if (!parse_rule_operator(operator_token, &action, &type)) {
        return {.ok = false, .error = "expected rule operator 's', 'h', 'ss', or 'hh'"};
    }

    std::string pattern;
    std::getline(input, pattern);
    pattern = trim(pattern);
    if (pattern.empty()) {
        return {.ok = false, .error = "missing rule pattern"};
    }

    try {
        return {.ok = true, .rule = Rule(action, type, pattern), .error = ""};
    } catch (const std::exception& ex) {
        return {.ok = false, .error = ex.what()};
    }
}

} // namespace lv
