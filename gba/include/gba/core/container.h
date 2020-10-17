#ifndef GAMEBOIADVANCE_CONTAINER_H
#define GAMEBOIADVANCE_CONTAINER_H

#include <array>
#include <vector>

#include <gba/core/integer.h>

namespace gba {

template<typename T, std::size_t N>
class array {
    std::array<T, N> data_;

public:
    array() = default;
    explicit array(std::array<T, N> data) : data_(data) {}

    [[nodiscard]] T& operator[](const u64 idx) noexcept { return data_[idx.get()]; }
    [[nodiscard]] T operator[](const u64 idx) const noexcept { return data_[idx.get()]; }
    [[nodiscard]] T* ptr(const u64 idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T& at(const u64 idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return data_.data(); }
    [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] u64 size() const noexcept { return u64(data_.size()); }

    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }
    auto cbegin() const noexcept { return data_.cbegin(); }
    auto cend() const noexcept { return data_.cend(); }

    std::array<T, N>& underlying_data() noexcept { return data_; }
    const std::array<T, N>& underlying_data() const noexcept { return data_; }
};

template<typename T>
class vector {
    std::vector<T> data_;

public:
    vector() = default;
    explicit vector(const u64 size) : data_(size.get()) {}
    explicit vector(std::vector<T> data) : data_(std::move(data)) {}

    [[nodiscard]] T& operator[](const u64 idx) noexcept { return data_[idx.get()]; }
    [[nodiscard]] T operator[](const u64 idx) const noexcept { return data_[idx.get()]; }
    [[nodiscard]] T* ptr(const u64 idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T& at(const u64 idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return data_.data(); }
    [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    template<typename... Args>
    T& emplace_back(Args... args) { return data_.emplace_back(std::forward<Args>(args)...); }
    void push_back(T t) { data_.push_back(std::move(t)); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] u64 size() const noexcept { return u64(data_.size()); }

    void clear() noexcept { data_.clear(); }
    auto erase(typename std::vector<T>::iterator pos) { return data_.erase(pos); }
    auto erase(typename std::vector<T>::const_iterator pos) { return data_.erase(pos); }
    auto erase(typename std::vector<T>::iterator first, typename std::vector<T>::iterator last) noexcept { return data_.erase(first, last); }
    auto erase(typename std::vector<T>::const_iterator first, typename std::vector<T>::const_iterator last) { return data_.erase(first, last); }
    void pop_back() noexcept { return data_.pop_back(); }

    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }
    auto cbegin() const noexcept { return data_.cbegin(); }
    auto cend() const noexcept { return data_.cend(); }

    std::vector<T>& underlying_data() noexcept { return data_; }
    const std::vector<T>& underlying_data() const noexcept { return data_; }
};

} // namespace gba

#endif //GAMEBOIADVANCE_CONTAINER_H
