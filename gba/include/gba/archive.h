/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_ARCHIVE_H
#define GAMEBOIADVANCE_ARCHIVE_H

#include <algorithm>
#include <type_traits>

#include <gba/core/container.h>

namespace gba {

class archive {
    vector<u8> data_;
    mutable usize read_pos_;

public:
    archive() = default;
    explicit archive(vector<u8> data)
      : data_{std::move(data)} {}

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] vector<u8> data() const noexcept { return data_; }
    void seek_to_start() noexcept { read_pos_ = 0_usize; }

    void clear() noexcept
    {
        data_.clear();
        seek_to_start();
    }

    template<typename T, usize::type N>
    void serialize(const array<T, N>& data) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "array of pointers are not supported");

        for(const T& e : data) {
            serialize(e);
        }
    }

    template<typename T>
    void serialize(const vector<T>& data) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "vector of pointers are not supported");

        serialize(data.size());
        for(const T& e : data) {
            serialize(e);
        }
    }

    template<typename T, usize::type N>
    void serialize(const static_vector<T, N>& data) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "static_vector of pointers are not supported");

        serialize(data.size());
        for(const T& e : data) {
            serialize(e);
        }
    }

    template<typename T>
    void serialize(const integer<T> data) noexcept
    {
        if constexpr(std::is_same_v<integer<T>, u8>) {
            data_.push_back(data);
        } else {
            write_bytes(make_byte_view(data));
        }
    }

    void serialize(const std::string_view& data) noexcept
    {
        const usize size = data.size();
        serialize(size);
        write_bytes(make_byte_view(data.data(), size));
    }

    template<typename T>
    void serialize(const T& t) noexcept
    {
        if constexpr(std::is_enum_v<T>) {
            using underlying_int = integer<std::underlying_type_t<T>>;
            serialize(from_enum<underlying_int>(t));
        } else if constexpr(std::is_floating_point_v<T>) {
            write_bytes(make_byte_view(t));
        } else if constexpr(std::is_same_v<T, bool>) {
            serialize(u8{static_cast<u8::type>(t)});
        } else {
            t.serialize(*this);
        }
    }

    /*****************************/

    template<typename T>
    T deserialize() const noexcept
    {
        T t;
        deserialize(t);
        return t;
    }

    template<typename T, usize::type N>
    void deserialize(array<T, N>& data) const noexcept
    {
        static_assert(!std::is_pointer_v<T>, "array of pointers are not supported");

        for(T& e : data) {
            deserialize(e);
        }
    }

    template<typename T>
    void deserialize(vector<T>& data) const noexcept
    {
        static_assert(!std::is_pointer_v<T>, "vector of pointers are not supported");

        const auto size = deserialize<usize>();

        data.resize(size);
        for(T& e : data) {
            deserialize(e);
        }
    }

    template<typename T, usize::type N>
    void deserialize(static_vector<T, N>& data) const noexcept
    {
        static_assert(!std::is_pointer_v<T>, "static_vector of pointers are not supported");

        data.clear();
        const auto size = deserialize<usize>();

        for(usize idx = 0_usize; idx < size; ++idx) {
            data.push_back(deserialize<T>());
        }
    }

    template<typename T>
    void deserialize(integer<T>& data) const noexcept
    {
        if constexpr(std::is_same_v<integer<T>, u8>) {
            data = data_[read_pos_++];
        } else {
            read_bytes(data);
        }
    }

    void deserialize(std::string_view& data) const noexcept
    {
        const auto size = deserialize<usize>();
        const u8* begin = data_.ptr(read_pos_);
        data = std::string_view{reinterpret_cast<const char*>(begin), size.get()};
        read_pos_ += size;
    }

    template<typename T>
    void deserialize(T& t) const noexcept
    {
        if constexpr(std::is_enum_v<T>) {
            using underlying_int = integer<std::underlying_type_t<T>>;
            t = to_enum<T>(deserialize<underlying_int>());
        } else if constexpr(std::is_floating_point_v<T>) {
            read_bytes(t);
        } else if constexpr(std::is_same_v<T, bool>) {
            t = static_cast<bool>(deserialize<u8>().get());
        } else {
            t.deserialize(*this);
        }
    }

private:
    FORCEINLINE void write_bytes(const view<u8> v) noexcept
    {
        std::copy(v.begin(), v.end(), std::back_inserter(data_));
    }

    template<typename T>
    FORCEINLINE void read_bytes(T& e) const noexcept
    {
        ASSERT(!data_.empty() && read_pos_ < data_.size());
        const u8* begin = data_.ptr(read_pos_);
        std::copy(begin, begin + sizeof(T), reinterpret_cast<u8*>(std::addressof(e))); // NOLINT
        read_pos_ += sizeof(T);
    }

    template<typename T>
    FORCEINLINE static view<u8> make_byte_view(const T* p, usize size) noexcept
    {
        constexpr usize elem_bytes = sizeof(T);
        return view<u8>{reinterpret_cast<const u8*>(p), size * elem_bytes}; // NOLINT
    }

    template<typename T>
    FORCEINLINE static view<u8> make_byte_view(const T& v) noexcept
    {
        constexpr usize elem_bytes = sizeof(T);
        return view<u8>{reinterpret_cast<const u8*>(&v), elem_bytes}; // NOLINT
    }
};

} // namespace gba

#endif  // GAMEBOIADVANCE_ARCHIVE_H
