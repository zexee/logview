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
            if (!rules[rule_index].enabled()) {
                continue;
            }
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

void FilterEngine::recompute_from(FilterResult& result, const LineIndex& index,
                                   const RuleSet& rules, std::size_t start_idx) const {
    for (LineNumber line_number = 0; line_number < index.line_count(); ++line_number) {
        bool visible = (start_idx == 0) ? true : result.layer(start_idx - 1).get(line_number);

        for (std::size_t rule_index = start_idx; rule_index < rules.size(); ++rule_index) {
            if (!rules[rule_index].enabled()) {
                continue;
            }
            bool bit = false;
            if (visible) {
                bit = rules[rule_index].passes(index.line(line_number));
            }
            result.layer(rule_index).set(line_number, bit);
            visible = bit;
        }

        result.final().set(line_number, visible);
    }
}

void FilterEngine::merge_final(FilterResult& result, const RuleSet& rules) const {
    const LineNumber line_count = result.line_count();
    for (LineNumber line_number = 0; line_number < line_count; ++line_number) {
        bool visible = true;
        for (std::size_t ri = 0; ri < rules.size(); ++ri) {
            if (!rules[ri].enabled()) {
                continue;
            }
            if (result.layer(ri).get(line_number)) {
                if (rules[ri].action() == RuleAction::Hide) {
                    visible = false;
                    break;
                }
            } else {
                if (rules[ri].action() == RuleAction::Show) {
                    visible = false;
                    break;
                }
            }
        }
        result.final().set(line_number, visible);
    }
}

} // namespace lv
