#pragma once

#include <regex>
#include <string>
#include <string_view>

namespace lv {

enum class RuleAction {
    Show,
    Hide,
};

enum class RuleMatchType {
    Literal,
    Regex,
};

class Rule {
public:
    Rule(RuleAction action, RuleMatchType type, std::string pattern);

    RuleAction action() const { return action_; }
    RuleMatchType type() const { return type_; }
    const std::string& pattern() const { return pattern_; }

    bool matches(std::string_view line) const;
    bool passes(std::string_view line) const;
    std::string serialize() const;

private:
    RuleAction action_;
    RuleMatchType type_;
    std::string pattern_;
    std::regex regex_;
};

const char* to_string(RuleAction action);
const char* to_string(RuleMatchType type);

} // namespace lv
