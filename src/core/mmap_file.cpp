#include "core/mmap_file.h"

#if defined(_WIN32)

#include <windows.h>

namespace lv {

MMapFile::~MMapFile() {
    close();
}

MMapFile::MMapFile(MMapFile&& other) noexcept {
    *this = std::move(other);
}

MMapFile& MMapFile::operator=(MMapFile&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    close();
    file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
    data_ = other.data_;
    size_ = other.size_;
    path_ = std::move(other.path_);
    other.file_handle_ = INVALID_HANDLE_VALUE;
    other.mapping_handle_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
}

bool MMapFile::open(const std::string& path) {
    close();

    // Convert UTF-8 path to UTF-16 for the wide Win32 API.
    if (path.empty()) {
        return false;
    }
    const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        return false;
    }
    std::wstring wpath(static_cast<std::size_t>(wlen), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    file_handle_ = ::CreateFileW(
        wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER file_size{};
    if (!::GetFileSizeEx(file_handle_, &file_size) || file_size.QuadPart < 0) {
        close();
        return false;
    }

    size_ = static_cast<std::size_t>(file_size.QuadPart);
    path_ = path;
    if (size_ == 0) {
        data_ = nullptr;
        return true;
    }

    mapping_handle_ = ::CreateFileMappingW(
        file_handle_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping_handle_ == nullptr) {
        close();
        return false;
    }

    void* mapped = ::MapViewOfFile(
        mapping_handle_, FILE_MAP_READ, 0, 0, 0);
    if (mapped == nullptr) {
        close();
        return false;
    }

    data_ = static_cast<const char*>(mapped);
    return true;
}

void MMapFile::close() {
    if (data_ != nullptr && size_ > 0) {
        ::UnmapViewOfFile(data_);
    }
    if (mapping_handle_ != nullptr) {
        ::CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
    data_ = nullptr;
    size_ = 0;
    path_.clear();
}

bool MMapFile::is_open() const {
    return file_handle_ != INVALID_HANDLE_VALUE;
}

} // namespace lv

#else // POSIX

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lv {

MMapFile::~MMapFile() {
    close();
}

MMapFile::MMapFile(MMapFile&& other) noexcept {
    *this = std::move(other);
}

MMapFile& MMapFile::operator=(MMapFile&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    close();
    fd_ = other.fd_;
    data_ = other.data_;
    size_ = other.size_;
    path_ = std::move(other.path_);
    other.fd_ = -1;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
}

bool MMapFile::open(const std::string& path) {
    close();

    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        return false;
    }

    struct stat st {};
    if (::fstat(fd_, &st) != 0 || st.st_size < 0) {
        close();
        return false;
    }

    size_ = static_cast<std::size_t>(st.st_size);
    path_ = path;
    if (size_ == 0) {
        data_ = nullptr;
        return true;
    }

    void* mapped = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapped == MAP_FAILED) {
        close();
        return false;
    }

    data_ = static_cast<const char*>(mapped);
    return true;
}

void MMapFile::close() {
    if (data_ != nullptr && size_ > 0) {
        ::munmap(const_cast<char*>(data_), size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
    data_ = nullptr;
    size_ = 0;
    path_.clear();
}

bool MMapFile::is_open() const {
    return fd_ >= 0;
}

} // namespace lv

#endif
