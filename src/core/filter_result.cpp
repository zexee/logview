#include "core/filter_result.h"

namespace lv {

FilterResult::FilterResult(LineNumber line_count, std::size_t rule_count) {
    resize(line_count, rule_count);
}

void FilterResult::resize(LineNumber line_count, std::size_t rule_count) {
    line_count_ = line_count;
    layers_.assign(rule_count, BitArray(line_count, false));
    final_.resize(line_count, false);
}

std::size_t FilterResult::bitmap_bytes() const {
    std::size_t bytes = final_.bytes();
    for (const BitArray& layer : layers_) {
        bytes += layer.bytes();
    }
    return bytes;
}

} // namespace lv
