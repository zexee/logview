#pragma once

#include <cstddef>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

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

    bool is_open() const;
    const char* data() const { return data_; }
    std::size_t size() const { return size_; }
    const std::string& path() const { return path_; }

private:
#if defined(_WIN32)
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle_ = nullptr;
#else
    int fd_ = -1;
#endif
    const char* data_ = nullptr;
    std::size_t size_ = 0;
    std::string path_;
};

} // namespace lv
