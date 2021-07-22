/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/helper/filesystem.h>

#include <fstream>
#include <memory>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gba::fs {

vector<u8> read_file(const path& path)
{
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if(!stream.is_open()) {
        LOG_CRITICAL(fs, "input file stream could not be opened: {}", path.string());
        std::terminate();
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
        std::terminate();
    }

    stream.write(reinterpret_cast<const char*>(data.data()), data.size().get()); // NOLINT
    LOG_TRACE(fs, "wrote {} bytes to {}", data.size(), path.string());
}

struct mmap::impl {
    using handle_type = HANDLE;
    static inline const handle_type invalid_handle = INVALID_HANDLE_VALUE; // NOLINT

    HANDLE file_handle = invalid_handle;
    HANDLE file_mapping_handle = invalid_handle;
    u8* map_ptr = nullptr;

    void map(const path& path, usize map_size, std::error_code& err) noexcept;
    void unmap() noexcept;
    void flush(usize flush_size) const noexcept;
    [[nodiscard]] bool is_mapped() const noexcept { return file_mapping_handle != invalid_handle; }
    [[nodiscard]] static int get_last_error() noexcept;
};

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
    unmap();
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
    if(!impl_) {
        impl_ = std::make_unique<impl>();
    }

    mapped_size_ = map_size;
    impl_->map(path_, map_size, err);

    if(is_mapped() && map_size == map_whole_file) {
        mapped_size_ = fs::file_size(path_);
    }
}

void mmap::unmap() noexcept
{
    if(impl_) {
        impl_->unmap();
        mapped_size_ = 0_usize;
    }
}

void mmap::flush() const noexcept
{
    if(impl_) {
        impl_->flush(mapped_size_);
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
    file_handle = CreateFileW(path.c_str(),
      GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
    if(file_handle == invalid_handle) {
        err.assign(impl::get_last_error(), std::system_category());
        LOG_ERROR(fs, "could not open file handle: {}, {}", err.message(), path.string());
        std::terminate();
    }

    file_mapping_handle = CreateFileMappingW(file_handle,
      nullptr, PAGE_READWRITE,
      map_size.get() >> 32, map_size.get(), nullptr);
    if(file_mapping_handle == invalid_handle) {
        err.assign(impl::get_last_error(), std::system_category());
        LOG_ERROR(fs, "could not create file mapping: {}, {}", err.message(), path.string());
        std::terminate();
    }

    map_ptr = static_cast<u8*>(MapViewOfFile(file_mapping_handle,
      FILE_MAP_ALL_ACCESS, 0, 0, map_size.get()));
}

void mmap::impl::unmap() noexcept
{
    if(is_mapped()) {
        UnmapViewOfFile(map_ptr);
        CloseHandle(file_mapping_handle);
        CloseHandle(file_handle);
        map_ptr = nullptr;
        file_mapping_handle = invalid_handle;
        file_handle = invalid_handle;
    }
}

void mmap::impl::flush(const usize flush_size) const noexcept
{
    FlushViewOfFile(map_ptr, flush_size.get());
}

int mmap::impl::get_last_error() noexcept
{
    return static_cast<int>(GetLastError());
}

} // namespace gba::fs
