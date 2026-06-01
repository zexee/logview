#include "core/line_index.h"

namespace lv {

bool LineIndex::build(const MMapFile& file) {
    data_ = file.data();
    size_ = file.size();
    line_starts_.clear();

    if (!file.is_open()) {
        return false;
    }
    if (size_ == 0) {
        return true;
    }

    line_starts_.push_back(0);
    for (std::size_t i = 0; i < size_; ++i) {
        if (data_[i] == '\n' && i + 1 < size_) {
            line_starts_.push_back(i + 1);
        }
    }
    return true;
}

std::string_view LineIndex::line(LineNumber number) const {
    if (data_ == nullptr || number >= line_starts_.size()) {
        return {};
    }
    const ByteOffset start = line_start(number);
    const ByteOffset end = line_end(number);
    return {data_ + start, end - start};
}

ByteOffset LineIndex::line_start(LineNumber number) const {
    if (number >= line_starts_.size()) {
        return size_;
    }
    return line_starts_[number];
}

ByteOffset LineIndex::line_end(LineNumber number) const {
    if (data_ == nullptr || number >= line_starts_.size()) {
        return 0;
    }
    const ByteOffset next_start = number + 1 < line_starts_.size()
                                      ? line_starts_[number + 1]
                                      : size_;
    ByteOffset end = next_start;
    while (end > line_starts_[number] && (data_[end - 1] == '\n' || data_[end - 1] == '\r')) {
        --end;
    }
    return end;
}

} // namespace lv
