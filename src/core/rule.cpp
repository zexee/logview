#include "core/rule.h"

#include <stdexcept>

namespace lv {

Rule::Rule(RuleAction action, RuleMatchType type, std::string pattern)
    : action_(action), type_(type), pattern_(std::move(pattern)), regex_() {
    if (pattern_.empty()) {
        throw std::invalid_argument("rule pattern must not be empty");
    }
    if (type_ == RuleMatchType::Regex) {
        regex_ = boost::regex(pattern_, boost::regex::ECMAScript | boost::regex::optimize);
    }
}

bool Rule::matches(std::string_view line) const {
    switch (type_) {
    case RuleMatchType::Literal:
        return line.find(pattern_) != std::string_view::npos;
    case RuleMatchType::Regex:
        return boost::regex_search(line.begin(), line.end(), regex_);
    }
    return false;
}

bool Rule::passes(std::string_view line) const {
    const bool matched = matches(line);
    return action_ == RuleAction::Show ? matched : !matched;
}

std::string Rule::serialize() const {
    if (action_ == RuleAction::Show && type_ == RuleMatchType::Regex) {
        return "s /" + pattern_ + "/";
    }
    if (action_ == RuleAction::Hide && type_ == RuleMatchType::Regex) {
        return "h /" + pattern_ + "/";
    }
    if (action_ == RuleAction::Show && type_ == RuleMatchType::Literal) {
        return "s " + pattern_;
    }
    return "h " + pattern_;
}

const char* to_string(RuleAction action) {
    switch (action) {
    case RuleAction::Show:
        return "show";
    case RuleAction::Hide:
        return "hide";
    }
    return "show";
}

const char* to_string(RuleMatchType type) {
    switch (type) {
    case RuleMatchType::Literal:
        return "literal";
    case RuleMatchType::Regex:
        return "regex";
    }
    return "literal";
}

} // namespace lv
