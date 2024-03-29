/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_CONTAINER_H
#define GAMEBOIADVANCE_CONTAINER_H

#include <cstring>  // std::memcpy
#include <type_traits>
#include <utility>
#include <vector>

#include <gba/core/integer.h>

namespace gba {

namespace detail {

// taken from MSVC's STL
template<typename First, typename... Rest>
struct array_enforce_same {
    static_assert(std::conjunction_v<std::is_same<First, Rest>...>,
      "N4687 26.3.7.2 [array.cons]/2: "
      "Requires: (is_same_v<T, U> && ...) is true. Otherwise the program is ill-formed.");
    using type = First;
};

} // namespace detail

template<typename T, usize::type N>
struct array {
    using value_type = T;
    using size_type = usize;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    T _data[N]; // NOLINT

    [[nodiscard]] constexpr T& operator[](const usize idx) noexcept { return _data[idx.get()]; }
    [[nodiscard]] constexpr const T& operator[](const usize idx) const noexcept { return _data[idx.get()]; }
    [[nodiscard]] constexpr T* ptr(const usize idx) noexcept { return &_data[idx.get()]; }
    [[nodiscard]] constexpr const T* ptr(const usize idx) const noexcept { return &_data[idx.get()]; }
    [[nodiscard]] constexpr T& at(const usize idx) noexcept { ASSERT(idx < N); return _data[idx.get()]; }
    [[nodiscard]] constexpr const T& at(const usize idx) const noexcept { ASSERT(idx < N); return _data[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return _data; }
    [[nodiscard]] const T* data() const noexcept { return _data; }

    [[nodiscard]] constexpr usize size() const noexcept { return N; }

    [[nodiscard]] T& front() noexcept { return at(0_usize); }
    [[nodiscard]] const T& front() const noexcept { return at(0_usize); }
    [[nodiscard]] T& back() noexcept { return at(size() - 1_usize); }
    [[nodiscard]] const T& back() const noexcept { return at(size() - 1_usize); }

    constexpr auto begin() noexcept { return std::begin(_data); }
    constexpr auto end() noexcept { return std::end(_data); }
    constexpr auto begin() const noexcept { return std::begin(_data); }
    constexpr auto end() const noexcept { return std::end(_data); }
    constexpr auto cbegin() const noexcept { return std::cbegin(_data); }
    constexpr auto cend() const noexcept { return std::cend(_data); }
};

template<typename First, typename... Rest>
array(First, Rest...) -> array<typename detail::array_enforce_same<First, Rest...>::type, 1 + sizeof...(Rest)>;

template<typename T>
class vector {
    std::vector<T> data_;

public:
    using value_type = T;
    using size_type = usize;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename std::vector<T>::pointer;
    using const_pointer = typename std::vector<T>::const_pointer;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    vector() = default;
    explicit vector(const usize size)
      : data_(size.get()) {}
    vector(const usize size, const T& init)
      : data_(size.get(), init) {}

    [[nodiscard]] T& operator[](const usize idx) noexcept { return data_[idx.get()]; }
    [[nodiscard]] const T& operator[](const usize idx) const noexcept { return data_[idx.get()]; }
    [[nodiscard]] T* ptr(const usize idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] const T* ptr(const usize idx) const noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T& at(const usize idx) noexcept { ASSERT(idx < size()); return data_[idx.get()]; }
    [[nodiscard]] const T& at(const usize idx) const noexcept { ASSERT(idx < size()); return data_[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return data_.data(); }
    [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    template<typename... Args>
    T& emplace_back(Args&&... args) { return data_.emplace_back(std::forward<Args>(args)...); }
    void push_back(const T& t) { data_.push_back(t); }
    void push_back(T&& t) { data_.push_back(std::move(t)); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] usize size() const noexcept { return data_.size(); }

    [[nodiscard]] T& front() noexcept { return at(0_usize); }
    [[nodiscard]] const T& front() const noexcept { return at(0_usize); }
    [[nodiscard]] T& back() noexcept { return at(size() - 1_usize); }
    [[nodiscard]] const T& back() const noexcept { return at(size() - 1_usize); }

    iterator erase(iterator pos) { return data_.erase(pos); }
    iterator erase(const_iterator pos) { return data_.erase(pos); }
    iterator erase(iterator first, iterator last) { return data_.erase(first, last); }
    iterator erase(const_iterator first, const_iterator last) { return data_.erase(first, last); }
    void pop_back() noexcept { data_.pop_back(); }

    void clear() noexcept { data_.clear(); }
    void resize(const usize new_size) { data_.resize(new_size.get()); }
    void reserve(const usize new_size) { data_.reserve(new_size.get()); }

    iterator begin() noexcept { return data_.begin(); }
    iterator end() noexcept { return data_.end(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }
};

template<typename T, usize::type Capacity>
class static_vector {
    static_assert(Capacity != 0, "Vector capacity must not be zero");

    array<std::aligned_storage_t<sizeof(T), alignof(T)>, Capacity> storage_;
    usize size_;

public:
    using value_type = T;
    using size_type = usize;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    static_vector() noexcept = default;
    ~static_vector() noexcept { destroy_range(begin(), end()); }
    static_vector(const static_vector&) noexcept = default;
    static_vector(static_vector&& other) noexcept :
      storage_{std::exchange(other.storage_, {})},
      size_{std::exchange(other.size_, 0_usize)} {}

    static_vector& operator=(const static_vector&) noexcept = default;
    static_vector& operator=(static_vector&& other) noexcept
    {
        storage_ = std::exchange(other.storage_, {});
        size_ = std::exchange(other.size_, 0_usize);
    }

    [[nodiscard]] T& operator[](const usize idx) noexcept { return *ptr(idx); }
    [[nodiscard]] const T& operator[](const usize idx) const noexcept { return *ptr(idx); }
    [[nodiscard]] T* ptr(const usize idx) noexcept { return reinterpret_cast<T*>(storage_.ptr(idx)); } // NOLINT
    [[nodiscard]] const T* ptr(const usize idx) const noexcept { return reinterpret_cast<const T*>(storage_.ptr(idx)); } // NOLINT
    [[nodiscard]] T& at(const usize idx) noexcept { ASSERT(idx < size_); return *ptr(idx); }
    [[nodiscard]] const T& at(const usize idx) const noexcept { ASSERT(idx < size_); return *ptr(idx); }
    [[nodiscard]] T* data() noexcept { return ptr(0_usize); }
    [[nodiscard]] const T* data() const noexcept { return ptr(0_usize); }

    template<typename... Args>
    constexpr T& emplace_back(Args&&... args) noexcept
    {
        static_assert(std::is_constructible_v<T, Args...>);
        ASSERT(size_ < Capacity);
        new (storage_.ptr(size_)) T(std::forward<Args>(args)...); // NOLINT
        return *ptr(size_++);
    }

    constexpr void push_back(const T& t) noexcept
    {
        ASSERT(size_ < Capacity);
        new (storage_.ptr(size_++)) T(t); // NOLINT
    }

    constexpr void push_back(T&& t) noexcept
    {
        ASSERT(size_ < Capacity);
        new (storage_.ptr(size_++)) T(std::move(t)); // NOLINT
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0_usize; }
    [[nodiscard]] constexpr usize size() const noexcept { return size_; }
    [[nodiscard]] constexpr usize capacity() const noexcept { return Capacity; }

    T& front() noexcept { return at(0_usize); }
    const T& front() const noexcept { return at(0_usize); }
    T& back() noexcept { return at(size() - 1_usize); }
    const T& back() const noexcept { return at(size() - 1_usize); }

    void erase(iterator first, iterator last) noexcept
    {
        destroy_range(first, last);
        std::rotate(first, last, end());
        size_ -= static_cast<usize::type>(std::distance(first, last));
    }

    void erase(iterator iter) noexcept { erase(iter, iter + 1); }

    constexpr void pop_back() noexcept { destroy_range(end() - 1, end()); --size_; }

    constexpr void clear() noexcept
    {
        destroy_range(begin(), end());
        size_ = 0_usize;
    }

    constexpr iterator begin() noexcept { return ptr(0_usize); }
    constexpr iterator end() noexcept { return ptr(size_); }
    constexpr const_iterator begin() const noexcept { return ptr(0_usize); }
    constexpr const_iterator end() const noexcept { return ptr(size_); }
    constexpr const_iterator cbegin() const noexcept { return ptr(0_usize); }
    constexpr const_iterator cend() const noexcept { return ptr(size_); }

private:
    void destroy_range([[maybe_unused]] T* first, [[maybe_unused]] T* last)
    {
        if constexpr(!std::is_trivially_destructible_v<T>) {
            ASSERT(first <= last);
            while(first != last) {
                first->~T();
                ++first; // NOLINT
            }
        }
    }
};

template<typename T>
class view {
    const T* entries_;
    usize size_;

public:
    using value_type = T;
    using size_type = usize;
    using reference = value_type&;
    using const_reference = const value_type&;
    using const_pointer = const T*;
    using const_iterator = const_pointer;

    template<typename Container>
    constexpr explicit view(const Container& container) noexcept
      : entries_{reinterpret_cast<const T*>(container.data())},
        size_{container.size() / sizeof(T)}
    {
        static_assert(std::is_same_v<u8, std::remove_reference_t<decltype(Container{}.front())>>,
          "source container element type must be u8");
        ASSERT((container.size() % sizeof(T)) == 0_usize); // alignment check
    }

    constexpr view(const T* data, const usize size)
      : entries_{data},
        size_{size} {}

    [[nodiscard]] constexpr const T& operator[](const usize idx) const noexcept { return *ptr(idx); }
    [[nodiscard]] constexpr const T* ptr(const usize idx) const noexcept { return entries_ + idx.get(); }
    [[nodiscard]] constexpr const T& at(const usize idx) const noexcept { ASSERT(idx < size_); return *ptr(idx); }
    [[nodiscard]] constexpr const T* data() const noexcept { return entries_; }

    [[nodiscard]] constexpr usize size() const noexcept { return size_; }

    [[nodiscard]] constexpr const T& front() const noexcept { return at(0_usize); }
    [[nodiscard]] constexpr const T& back() const noexcept { return at(size() - 1_usize); }

    constexpr const_iterator begin() const noexcept { return ptr(0_usize); }
    constexpr const_iterator end() const noexcept { return ptr(size_); }
    constexpr const_iterator cbegin() const noexcept { return ptr(0_usize); }
    constexpr const_iterator cend() const noexcept { return ptr(size_); }
};

template<typename T, typename Container>
[[nodiscard]] FORCEINLINE T memcpy(Container&& container, const usize offset) noexcept
{
    T value;
    std::memcpy(&value, container.ptr(offset), sizeof(T));
    return value;
}

template<typename T, typename Container>
FORCEINLINE void memcpy(Container&& container, const usize offset, const T& value) noexcept
{
    std::memcpy(container.ptr(offset), &value, sizeof(T));
}

} // namespace gba

#endif //GAMEBOIADVANCE_CONTAINER_H
