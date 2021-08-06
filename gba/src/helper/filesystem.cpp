/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/helper/filesystem.h>

#include <fstream>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <Windows.h>
#else
  #include <errno.h>
  #include <fcntl.h>    // open
  #include <sys/mman.h> // mmap, munmap
#endif // _WIN32

namespace gba::fs {

vector<u8> read_file(const path& path)
{
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if(!stream.is_open()) {
        LOG_CRITICAL(fs, "input file stream could not be opened: {}", path.string());
        PANIC();
    }

    const std::ifstream::pos_type file_size = stream.tellg();

    vector<u8> bytes{static_cast<usize::type>(file_size)};
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), file_size); // NOLINT

    LOG_TRACE(fs, "read {} bytes from {}", bytes.size(), path.string());
    return bytes;
}

void write_file(const path& path, const vector<u8>& data)
{
    write_file(path, view<u8>{data.data(), data.size()});
}

void write_file(const path& path, const view<u8> data)
{
    std::ofstream stream{path, std::ios::binary};
    if(!stream.is_open()) {
        LOG_CRITICAL(fs, "output file stream could not be opened: {}", path.string());
        PANIC();
    }

    stream.write(reinterpret_cast<const char*>(data.data()), data.size().get()); // NOLINT
    LOG_TRACE(fs, "wrote {} bytes to {}", data.size(), path.string());
}

struct mmap::impl {
#ifdef _WIN32
    using handle_type = HANDLE;
    static inline const handle_type invalid_handle = INVALID_HANDLE_VALUE; // NOLINT

    handle_type file_handle = invalid_handle;
    handle_type file_mapping_handle = invalid_handle;
#else
    using handle_type = int;
    static inline const handle_type invalid_handle = -1;

    handle_type file_handle = invalid_handle;
#endif // _WIN32

    u8* map_ptr = nullptr;

    void map(const path& path, usize map_size, std::error_code& err) noexcept;
    void unmap(usize map_size, std::error_code& err) noexcept;
    void flush(usize flush_size, std::error_code& err) const noexcept;
    [[nodiscard]] bool is_mapped() const noexcept { return map_ptr != nullptr; }
};

namespace {

void populate_err(std::error_code& err) noexcept
{
#ifdef _WIN32
    err.assign(static_cast<int>(GetLastError()), std::system_category());
#else
    err.assign(errno, std::system_category());
#endif // _WIN32
}

} // namespace

mmap::mmap() noexcept {}

mmap::mmap(std::filesystem::path path, std::error_code& err) noexcept
  : mmap(std::move(path), map_whole_file, err) {}

mmap::mmap(path path, const usize map_size, std::error_code& err) noexcept
  : path_{std::move(path)},
    mapped_size_{map_size},
    impl_{std::make_unique<impl>()}
{
    map(map_size, err);
}

mmap::~mmap() noexcept
{
    std::error_code err; // discarded
    unmap(err);
}

const u8* mmap::ptr(const usize idx) const noexcept
{
    ASSERT(impl_ != nullptr);
    return impl_->map_ptr + idx.get(); // NOLINT
}

u8* mmap::ptr(const usize idx) noexcept
{
    ASSERT(impl_ != nullptr);
    return impl_->map_ptr + idx.get(); // NOLINT
}

mmap::mmap(mmap&& other) noexcept
  : path_{std::move(other.path_)},
    mapped_size_{std::exchange(other.mapped_size_, 0_usize)},
    impl_{std::move(other.impl_)} {}

mmap& mmap::operator=(mmap&& other) noexcept
{
    path_ = std::move(other.path_);
    mapped_size_ = std::exchange(other.mapped_size_, 0_usize);
    impl_ = std::move(other.impl_);
    return *this;
}

void mmap::map(std::error_code& err) noexcept
{
    map(map_whole_file, err);
}

void mmap::map(const usize map_size, std::error_code& err) noexcept
{
    if(!exists(path_)) {
        err = std::make_error_code(std::errc::no_such_file_or_directory);
        return;
    }

    if(!impl_) {
        impl_ = std::make_unique<impl>();
    }

    if(map_size == map_whole_file) {
        mapped_size_ = file_size(path_);
    } else {
        mapped_size_ = map_size;
    }

    impl_->map(path_, map_size, err);
}

void mmap::unmap(std::error_code& err) noexcept
{
    if(impl_) {
        impl_->unmap(mapped_size_, err);
        mapped_size_ = 0_usize;
    }
}

void mmap::flush(std::error_code& err) const noexcept
{
    if(impl_) {
        impl_->flush(mapped_size_, err);
    }
}

bool mmap::is_mapped() const noexcept
{
    if(impl_) {
        return impl_->is_mapped();
    }
    return false;
}

void mmap::impl::map(const path& path, usize map_size, std::error_code& err) noexcept
{
    if(is_mapped()) {
        err = std::make_error_code(std::errc::already_connected);
        return;
    }

#ifdef _WIN32
    file_handle = CreateFileW(path.c_str(),
      GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
    if(file_handle == invalid_handle) {
        populate_err(err);
        return;
    }

    file_mapping_handle = CreateFileMappingW(file_handle,
      nullptr, PAGE_READWRITE,
      map_size.get() >> 32, map_size.get(), nullptr);
    if(file_mapping_handle == invalid_handle) {
        populate_err(err);
        return;
    }

    map_ptr = static_cast<u8*>(MapViewOfFile(file_mapping_handle,
      FILE_MAP_ALL_ACCESS, 0, 0, map_size.get()));
    if(!map_ptr) {
        populate_err(err);
    }
#else
    file_handle = ::open(path.c_str(), O_RDWR);
    if(file_handle == invalid_handle) {
        populate_err(err);
        return;
    }

    map_ptr = static_cast<u8*>(::mmap(nullptr, map_size.size(),
      PROT_READ | PROT_WRITE, MAP_SHARED, file_handle, 0));
    if(map_ptr == MAP_FAILED) {
        populate_err(err);
    }
#endif // _WIN32
}

void mmap::impl::unmap([[maybe_unused]] const usize map_size, std::error_code& err) noexcept
{
    if(!is_mapped()) {
        err = std::make_error_code(std::errc::not_connected);
        return;
    }

#ifdef _WIN32
    const bool not_unmapped = UnmapViewOfFile(map_ptr) == 0;
    const bool mapping_not_closed = CloseHandle(file_mapping_handle) == 0;
    const bool file_not_closed = CloseHandle(file_handle) == 0;
    if(not_unmapped || mapping_not_closed || file_not_closed) {
        populate_err(err);
    }

    file_mapping_handle = invalid_handle;
#else
    const bool not_unmapped = ::munmap(map_ptr, map_size.size()) == -1;
    const bool file_not_closed = ::close(file_handle) == -1;
    if(not_unmapped || file_not_closed) {
        populate_err(err);
    }
#endif // _WIN32

    file_handle = invalid_handle;
    map_ptr = nullptr;
}

void mmap::impl::flush(const usize flush_size, std::error_code& err) const noexcept
{
#ifdef _WIN32
    if(FlushViewOfFile(map_ptr, flush_size.get()) == 0 || FlushFileBuffers(file_handle) == 0) {
        populate_err(err);
    }
#else
    if(::msync(map_ptr, flush_size.get(), MS_SYNC) == -1) {
        populate_err_and_log(err, "could not flush mmap");
    }
#endif // _WIN32
}

} // namespace gba::fs
