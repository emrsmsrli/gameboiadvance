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
    mutable usize read_pos;

    struct byte_view {
        const u8* start = nullptr;
        usize size;
    };

public:
#if DEBUG
    enum class state_type : u8::type { array, vector, integer };
#endif // DEBUG

    archive() = default;
    explicit archive(vector<u8> data)
      : data_{std::move(data)} {}

    [[nodiscard]] const vector<u8>& data() const noexcept { return data_; }
    void clear() noexcept { data_.clear(); read_pos = 0_usize; }

    template<typename T, usize::type N>
    void serialize(const array<T, N>& data) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "array of pointers are not supported");

#if DEBUG
        debug_write_type(state_type::array);
#endif // DEBUG

        for(const T& e : data) {
            serialize(e);
        }
    }

    template<typename T>
    void serialize(const vector<T>& data) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "vector of pointers are not supported");

#if DEBUG
        debug_write_type(state_type::vector);
#endif // DEBUG

        serialize(data.size());
        for(const T& e : data) {
            serialize(e);
        }
    }

    template<typename T>
    void serialize(const integer<T> data) noexcept
    {
#if DEBUG
        debug_write_type(state_type::integer);
#endif // DEBUG

        if constexpr(std::is_same_v<integer<T>, u8>) {
            data_.push_back(data);
        } else {
            write_bytes(make_byte_view(data));
        }
    }

    template<typename T>
    void serialize(const T& t) noexcept { t.serialize(*this); }

    /*****************************/

    template<typename T, usize::type N>
    void deserialize(array<T, N>& data) const noexcept
    {
        static_assert(!std::is_pointer_v<T>, "array of pointers are not supported");

#if DEBUG
        debug_assert_type(state_type::array);
#endif // DEBUG

        for(T& e : data) {
            deserialize(e);
        }
    }

    template<typename T>
    void deserialize(vector<T>& data) const noexcept
    {
        static_assert(!std::is_pointer_v<T>, "vector of pointers are not supported");

#if DEBUG
        debug_assert_type(state_type::vector);
#endif // DEBUG

        usize size;
        deserialize(size);

        data.resize(size);
        for(T& e : data) {
            deserialize(e);
        }
    }

    template<typename T>
    void deserialize(integer<T>& data) const noexcept
    {
#if DEBUG
        debug_assert_type(state_type::integer);
#endif // DEBUG

        if constexpr(std::is_same_v<integer<T>, u8>) {
            data = data_[read_pos++];
        } else {
            read_bytes(data);
        }
    }

    template<typename T>
    void deserialize(T& t) const noexcept { t.deserialize(*this); }

private:
    FORCEINLINE void write_bytes(const byte_view v) noexcept
    {
        std::copy(v.start, v.start + v.size.get(), std::back_inserter(data_)); // NOLINT
    }

    template<typename T>
    FORCEINLINE void read_bytes(T& e) const noexcept
    {
        ASSERT(!data_.empty() && read_pos < data_.size());
        const u8* begin = data_.data() + read_pos.get(); // NOLINT
        std::copy(begin, begin + sizeof(T), reinterpret_cast<u8*>(std::addressof(e))); // NOLINT
        read_pos += sizeof(T);
    }

#if DEBUG
    FORCEINLINE void debug_write_type(const state_type type) noexcept
    {
        data_.push_back(from_enum<u8>(type));
    }

    FORCEINLINE void debug_assert_type(const state_type type) const noexcept
    {
        ASSERT(to_enum<state_type>(data_[read_pos++]) == type);
    }
#endif // DEBUG

    template<typename T>
    FORCEINLINE static byte_view make_byte_view(const T* p, usize size) noexcept
    {
        constexpr usize elem_bytes = sizeof(T);
        return byte_view{reinterpret_cast<const u8*>(p), size * elem_bytes}; // NOLINT
    }

    template<typename T>
    FORCEINLINE static byte_view make_byte_view(const T& v) noexcept
    {
        constexpr usize elem_bytes = sizeof(T);
        return byte_view{reinterpret_cast<const u8*>(&v), elem_bytes}; // NOLINT
    }
};

} // namespace gba

#endif  // GAMEBOIADVANCE_ARCHIVE_H
