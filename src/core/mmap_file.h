#pragma once

#include <cstddef>
#include <string>

namespace lv {

class MMapFile {
public:
    MMapFile() = default;
    ~MMapFile();

    MMapFile(const MMapFile&) = delete;
    MMapFile& operator=(const MMapFile&) = delete;

    MMapFile(MMapFile&& other) noexcept;
    MMapFile& operator=(MMapFile&& other) noexcept;

    bool open(const std::string& path);
    void close();

    bool is_open() const { return fd_ >= 0; }
    const char* data() const { return data_; }
    std::size_t size() const { return size_; }
    const std::string& path() const { return path_; }

private:
    int fd_ = -1;
    const char* data_ = nullptr;
    std::size_t size_ = 0;
    std::string path_;
};

} // namespace lv
