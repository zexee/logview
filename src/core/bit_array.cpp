#include "core/bit_array.h"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace lv {

BitArray::BitArray(std::size_t bit_count, bool value) {
    resize(bit_count, value);
}

void BitArray::resize(std::size_t bit_count, bool value) {
    bit_count_ = bit_count;
    const std::size_t words = (bit_count + 63) / 64;
    words_.assign(words, value ? ~std::uint64_t{0} : std::uint64_t{0});
    if (value) {
        mask_unused_bits();
    }
}

void BitArray::set(std::size_t index, bool value) {
    if (index >= bit_count_) {
        throw std::out_of_range("BitArray::set index out of range");
    }
    const std::size_t word = index / 64;
    const std::uint64_t mask = std::uint64_t{1} << (index % 64);
    if (value) {
        words_[word] |= mask;
    } else {
        words_[word] &= ~mask;
    }
}

bool BitArray::get(std::size_t index) const {
    if (index >= bit_count_) {
        return false;
    }
    return (words_[index / 64] & (std::uint64_t{1} << (index % 64))) != 0;
}

void BitArray::fill(bool value) {
    std::fill(words_.begin(), words_.end(), value ? ~std::uint64_t{0} : std::uint64_t{0});
    if (value) {
        mask_unused_bits();
    }
}

std::size_t BitArray::count_ones() const {
    std::size_t total = 0;
    for (std::uint64_t word : words_) {
        total += static_cast<std::size_t>(std::popcount(word));
    }
    return total;
}

void BitArray::mask_unused_bits() {
    if (words_.empty()) {
        return;
    }
    const std::size_t used_bits = bit_count_ % 64;
    if (used_bits == 0) {
        return;
    }
    words_.back() &= (std::uint64_t{1} << used_bits) - 1;
}

} // namespace lv
