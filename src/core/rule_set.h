#pragma once

#include "core/rule.h"

#include <string>
#include <vector>

namespace lv {

struct RuleParseResult {
    bool ok = false;
    Rule rule = Rule(RuleAction::Show, RuleMatchType::Literal, "__placeholder__");
    std::string error;
};

class RuleSet {
public:
    bool load(const std::string& path, std::string* error = nullptr);
    bool save(const std::string& path, std::string* error = nullptr) const;

    void add(Rule rule);
    bool insert(std::size_t index, Rule rule);
    bool replace(std::size_t index, Rule rule);
    bool remove(std::size_t index);
    bool move_up(std::size_t index);
    bool move_down(std::size_t index);
    void clear();

    std::size_t size() const { return rules_.size(); }
    bool empty() const { return rules_.empty(); }
    const Rule& operator[](std::size_t index) const { return rules_[index]; }
    const std::vector<Rule>& rules() const { return rules_; }

    static RuleParseResult parse_line(const std::string& line);

private:
    std::vector<Rule> rules_;
};

} // namespace lv
