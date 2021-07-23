/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_FILESYSTEM_H
#define GAMEBOIADVANCE_FILESYSTEM_H

#include <filesystem>
#include <memory>
#include <system_error>

#include <gba/core/container.h>

namespace gba::fs {

using namespace std::filesystem;

vector<u8> read_file(const path& path);
void write_file(const path& path, const vector<u8>& data);
void write_file(const path& path, view<u8> data);

class mmap {
    fs::path path_;
    usize mapped_size_;

    struct impl;
    std::unique_ptr<impl> impl_ = nullptr;

public:
    static inline constexpr auto map_whole_file = 0_usize;

    using value_type = u8;
    using size_type = usize;
    using reference = u8&;
    using const_reference = const u8&;
    using pointer = u8*;
    using const_pointer = const u8*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    mmap() noexcept;
    mmap(fs::path path, std::error_code& err) noexcept;
    mmap(fs::path path, usize map_size, std::error_code& err) noexcept;

    ~mmap() noexcept;

    // move only
    mmap(const mmap&) = delete;
    mmap(mmap&&) noexcept;

    mmap& operator=(const mmap&) = delete;
    mmap& operator=(mmap&&) noexcept;

    [[nodiscard]] u8& operator[](const usize idx) noexcept { return *ptr(idx); }
    [[nodiscard]] u8 operator[](const usize idx) const noexcept { return *ptr(idx); }
    [[nodiscard]] u8* ptr(usize idx) noexcept;
    [[nodiscard]] const u8* ptr(usize idx) const noexcept;
    [[nodiscard]] u8& at(const usize idx) noexcept { ASSERT(idx < mapped_size_); return *ptr(idx); }
    [[nodiscard]] u8 at(const usize idx) const noexcept { ASSERT(idx < mapped_size_); return *ptr(idx); }
    [[nodiscard]] u8* data() noexcept { return ptr(0_usize); }
    [[nodiscard]] const u8* data() const noexcept { return ptr(0_usize); }

    [[nodiscard]] constexpr usize size() const noexcept { return mapped_size_; }

    [[nodiscard]] u8& front() noexcept { return at(0_usize); }
    [[nodiscard]] u8 front() const noexcept { return at(0_usize); }
    [[nodiscard]] u8& back() noexcept { return at(size() - 1_usize); }
    [[nodiscard]] u8 back() const noexcept { return at(size() - 1_usize); }

    void map(std::error_code& err) noexcept;
    void map(usize map_size, std::error_code& err) noexcept;
    void unmap(std::error_code& err) noexcept;
    void flush(std::error_code& err) const noexcept;
    [[nodiscard]] bool is_mapped() const noexcept;

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] iterator end() noexcept { return ptr(mapped_size_); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator end() const noexcept { return ptr(mapped_size_); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
    [[nodiscard]] const_iterator cend() const noexcept { return ptr(mapped_size_); }
};

} // namespace gba::fs

#endif //GAMEBOIADVANCE_FILESYSTEM_H
