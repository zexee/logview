#pragma once

#include "core/filter_result.h"
#include "core/line_index.h"
#include "core/rule_set.h"

namespace lv {

class FilterEngine {
public:
    FilterResult run(const LineIndex& index, const RuleSet& rules) const;
};

} // namespace lv
