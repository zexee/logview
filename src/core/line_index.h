#pragma once

#include "core/mmap_file.h"
#include "core/types.h"

#include <string_view>
#include <vector>

namespace lv {

class LineIndex {
public:
    bool build(const MMapFile& file);

    LineNumber line_count() const { return line_starts_.size(); }
    bool empty() const { return line_starts_.empty(); }
    std::string_view line(LineNumber number) const;
    ByteOffset line_start(LineNumber number) const;
    ByteOffset line_end(LineNumber number) const;

    const std::vector<ByteOffset>& starts() const { return line_starts_; }

private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
    std::vector<ByteOffset> line_starts_;
};

} // namespace lv
