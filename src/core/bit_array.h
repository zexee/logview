#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lv {

class BitArray {
public:
    BitArray() = default;
    explicit BitArray(std::size_t bit_count, bool value = false);

    void resize(std::size_t bit_count, bool value = false);
    void set(std::size_t index, bool value);
    bool get(std::size_t index) const;
    void fill(bool value);

    std::size_t size() const { return bit_count_; }
    bool empty() const { return bit_count_ == 0; }
    std::size_t word_count() const { return words_.size(); }
    std::size_t bytes() const { return words_.size() * sizeof(std::uint64_t); }
    std::size_t count_ones() const;

    const std::vector<std::uint64_t>& words() const { return words_; }
    std::vector<std::uint64_t>& words() { return words_; }

private:
    void mask_unused_bits();

    std::size_t bit_count_ = 0;
    std::vector<std::uint64_t> words_;
};

} // namespace lv
