#include "core/rule.h"

#include <stdexcept>

namespace lv {

Rule::Rule(RuleAction action, std::vector<RuleSegment> segments)
    : action_(action), segments_(std::move(segments)) {
    if (segments_.empty()) {
        throw std::invalid_argument("rule must have at least one segment");
    }
    for (RuleSegment& seg : segments_) {
        if (seg.pattern.empty()) {
            throw std::invalid_argument("rule segment pattern must not be empty");
        }
        if (seg.type == RuleMatchType::Regex) {
            seg.regex = boost::regex(seg.pattern, boost::regex::ECMAScript | boost::regex::optimize);
        }
    }
}

bool Rule::matches(std::string_view line) const {
    for (const RuleSegment& seg : segments_) {
        switch (seg.type) {
        case RuleMatchType::Literal:
            if (line.find(seg.pattern) != std::string_view::npos) {
                return true;
            }
            break;
        case RuleMatchType::Regex:
            if (seg.regex && boost::regex_search(line.begin(), line.end(), *seg.regex)) {
                return true;
            }
            break;
        }
    }
    return false;
}

bool Rule::passes(std::string_view line) const {
    const bool matched = matches(line);
    return action_ == RuleAction::Show ? matched : !matched;
}

std::string Rule::serialize() const {
    std::string result = enabled_ ? "" : "-";
    if (action_ == RuleAction::Show) {
        result += "s ";
    } else {
        result += "h ";
    }
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        if (i > 0) {
            result += "|";
        }
        const RuleSegment& seg = segments_[i];
        if (seg.type == RuleMatchType::Regex) {
            result += "/" + seg.pattern + "/";
        } else {
            result += seg.pattern;
        }
    }
    return result;
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
