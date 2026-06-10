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

void FilterResult::remove_layer(std::size_t index) {
    if (index < layers_.size()) {
        layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void FilterResult::insert_layer(std::size_t index) {
    if (index <= layers_.size()) {
        layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(index), BitArray(line_count_, false));
    }
}

void FilterResult::add_layer() {
    layers_.emplace_back(line_count_, false);
}

} // namespace lv
