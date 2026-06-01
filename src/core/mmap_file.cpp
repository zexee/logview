#include "core/mmap_file.h"

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

} // namespace lv
