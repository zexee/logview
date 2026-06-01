#include "core/filter_engine.h"

namespace lv {

FilterResult FilterEngine::run(const LineIndex& index, const RuleSet& rules) const {
    FilterResult result(index.line_count(), rules.size());

    if (rules.empty()) {
        result.final().fill(true);
        return result;
    }

    for (LineNumber line_number = 0; line_number < index.line_count(); ++line_number) {
        const std::string_view line = index.line(line_number);
        bool visible = true;

        for (std::size_t rule_index = 0; rule_index < rules.size(); ++rule_index) {
            bool bit = false;
            if (visible) {
                bit = rules[rule_index].passes(line);
            }
            result.layer(rule_index).set(line_number, bit);
            visible = bit;
        }

        result.final().set(line_number, visible);
    }

    return result;
}

} // namespace lv
