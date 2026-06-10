#pragma once

#include "core/bit_array.h"
#include "core/types.h"

#include <cstddef>
#include <vector>

namespace lv {

class FilterResult {
public:
    FilterResult() = default;
    FilterResult(LineNumber line_count, std::size_t rule_count);

    void resize(LineNumber line_count, std::size_t rule_count);

    LineNumber line_count() const { return line_count_; }
    std::size_t rule_count() const { return layers_.size(); }
    bool visible(LineNumber line) const { return final_.get(line); }
    std::size_t visible_count() const { return final_.count_ones(); }
    std::size_t bitmap_bytes() const;

    const BitArray& layer(std::size_t index) const { return layers_[index]; }
    BitArray& layer(std::size_t index) { return layers_[index]; }
    const BitArray& final() const { return final_; }
    BitArray& final() { return final_; }

    void remove_layer(std::size_t index);
    void insert_layer(std::size_t index);
    void add_layer();

private:
    LineNumber line_count_ = 0;
    std::vector<BitArray> layers_;
    BitArray final_;
};

} // namespace lv
