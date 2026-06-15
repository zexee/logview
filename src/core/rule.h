#pragma once

#include <boost/regex.hpp>
#include "core/types.h"
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
    LineRange,
};

struct RuleSegment {
    RuleMatchType type;
    std::string pattern;
    std::optional<boost::regex> regex;
    LineNumber line_start = 0;
    LineNumber line_end = 0;
    bool has_line_end = false;
};

class Rule {
public:
    Rule(RuleAction action, std::vector<RuleSegment> segments, bool case_insensitive = false);

    RuleAction action() const { return action_; }
    const std::vector<RuleSegment>& segments() const { return segments_; }
    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool case_insensitive() const { return case_insensitive_; }
    void set_case_insensitive(bool val) { case_insensitive_ = val; }

    bool matches(std::string_view line, LineNumber line_number = 0, LineNumber total_lines = 0) const;
    bool passes(std::string_view line, LineNumber line_number = 0, LineNumber total_lines = 0) const;
    std::string serialize() const;

private:
    RuleAction action_;
    std::vector<RuleSegment> segments_;
    bool enabled_ = true;
    bool case_insensitive_ = false;
};

const char* to_string(RuleAction action);
const char* to_string(RuleMatchType type);

} // namespace lv
