#pragma once

#include <boost/regex.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lv {

enum class RuleAction {
    Show,
    Hide,
};

enum class RuleMatchType {
    Literal,
    Regex,
};

struct RuleSegment {
    RuleMatchType type;
    std::string pattern;
    std::optional<boost::regex> regex;
};

class Rule {
public:
    Rule(RuleAction action, std::vector<RuleSegment> segments);

    RuleAction action() const { return action_; }
    const std::vector<RuleSegment>& segments() const { return segments_; }

    bool matches(std::string_view line) const;
    bool passes(std::string_view line) const;
    std::string serialize() const;

private:
    RuleAction action_;
    std::vector<RuleSegment> segments_;
};

const char* to_string(RuleAction action);
const char* to_string(RuleMatchType type);

} // namespace lv
