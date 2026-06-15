#include "core/rule.h"

#include <cctype>
#include <stdexcept>

namespace lv {

Rule::Rule(RuleAction action, std::vector<RuleSegment> segments, bool case_insensitive)
    : action_(action), segments_(std::move(segments)), case_insensitive_(case_insensitive) {
    if (segments_.empty()) {
        throw std::invalid_argument("rule must have at least one segment");
    }
    for (RuleSegment& seg : segments_) {
        if (seg.type != RuleMatchType::LineRange && seg.pattern.empty()) {
            throw std::invalid_argument("rule segment pattern must not be empty");
        }
        if (seg.type == RuleMatchType::Regex) {
            auto flags = boost::regex::ECMAScript | boost::regex::optimize;
            if (case_insensitive_) {
                flags |= boost::regex::icase;
            }
            seg.regex = boost::regex(seg.pattern, flags);
        }
    }
}

bool Rule::matches(std::string_view line, LineNumber line_number, LineNumber total_lines) const {
    for (const RuleSegment& seg : segments_) {
        switch (seg.type) {
        case RuleMatchType::Literal:
            if (case_insensitive_) {
                auto eq = [](unsigned char a, unsigned char b) {
                    return std::tolower(a) == std::tolower(b);
                };
                auto it = std::search(line.begin(), line.end(),
                                      seg.pattern.begin(), seg.pattern.end(), eq);
                if (it != line.end()) {
                    return true;
                }
            } else {
                if (line.find(seg.pattern) != std::string_view::npos) {
                    return true;
                }
            }
            break;
        case RuleMatchType::Regex:
            if (seg.regex && boost::regex_search(line.begin(), line.end(), *seg.regex)) {
                return true;
            }
            break;
        case RuleMatchType::LineRange: {
            std::int64_t one_based = static_cast<std::int64_t>(line_number) + 1;
            std::int64_t start = static_cast<std::int64_t>(seg.line_start);
            if (start < 0) {
                start = static_cast<std::int64_t>(total_lines) + start + 1;
            }
            std::int64_t end;
            if (!seg.has_line_end) {
                end = static_cast<std::int64_t>(total_lines);
            } else {
                end = static_cast<std::int64_t>(seg.line_end);
                if (end < 0) {
                    end = static_cast<std::int64_t>(total_lines) + end + 1;
                }
            }
            if (one_based >= start && one_based <= end) {
                return true;
            }
            break;
        }
        }
    }
    return false;
}

bool Rule::passes(std::string_view line, LineNumber line_number, LineNumber total_lines) const {
    const bool matched = matches(line, line_number, total_lines);
    return action_ == RuleAction::Show ? matched : !matched;
}

std::string Rule::serialize() const {
    std::string result = enabled_ ? "" : "-";
    const RuleSegment& seg = segments_[0];
    if (seg.type == RuleMatchType::LineRange) {
        if (action_ == RuleAction::Show) {
            result += "sl ";
        } else {
            result += "hl ";
        }
        result += std::to_string(seg.line_start);
        if (seg.has_line_end) {
            if (static_cast<std::int64_t>(seg.line_end) < 0) {
                result += " " + std::to_string(static_cast<std::int64_t>(seg.line_end));
            } else {
                result += " " + std::to_string(seg.line_end);
            }
        }
        return result;
    }

    if (action_ == RuleAction::Show) {
        result += case_insensitive_ ? "si " : "s ";
    } else {
        result += case_insensitive_ ? "hi " : "h ";
    }
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        if (i > 0) {
            result += "|";
        }
        const RuleSegment& s = segments_[i];
        if (s.type == RuleMatchType::Regex) {
            result += "/" + s.pattern + "/";
        } else {
            result += s.pattern;
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
    case RuleMatchType::LineRange:
        return "linerange";
    }
    return "literal";
}

} // namespace lv
