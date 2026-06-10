#pragma once

#include "core/filter_result.h"
#include "core/line_index.h"
#include "core/rule_set.h"

namespace lv {

class FilterEngine {
public:
    FilterResult run(const LineIndex& index, const RuleSet& rules) const;
    void recompute_from(FilterResult& result, const LineIndex& index, const RuleSet& rules, std::size_t start_idx) const;
    void merge_final(FilterResult& result, const RuleSet& rules) const;
};

} // namespace lv
