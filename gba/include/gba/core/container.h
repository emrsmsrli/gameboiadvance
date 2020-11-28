#ifndef GAMEBOIADVANCE_CONTAINER_H
#define GAMEBOIADVANCE_CONTAINER_H

#include <vector>
#include <cstring>  // std::memcpy
#include <utility>  // std::begin, std::end
#include <type_traits>

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
    T _data[N]; // NOLINT

    [[nodiscard]] constexpr T& operator[](const usize idx) noexcept { return _data[idx.get()]; }
    [[nodiscard]] constexpr T operator[](const usize idx) const noexcept { return _data[idx.get()]; }
    [[nodiscard]] constexpr T* ptr(const usize idx) noexcept { return &_data[idx.get()]; }
    [[nodiscard]] constexpr const T* ptr(const usize idx) const noexcept { return &_data[idx.get()]; }
    [[nodiscard]] constexpr T& at(const usize idx) noexcept { return _data[idx.get()]; }
    [[nodiscard]] constexpr T at(const usize idx) const noexcept { return _data[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return _data; }
    [[nodiscard]] const T* data() const noexcept { return _data; }

    [[nodiscard]] constexpr usize size() const noexcept { return N; }

    constexpr auto begin() noexcept { return std::begin(_data); }
    constexpr auto end() noexcept { return std::end(_data); }
    constexpr auto begin() const noexcept { return std::begin(_data); }
    constexpr auto end() const noexcept { return std::end(_data); }
    constexpr auto cbegin() const noexcept { return std::cbegin(_data); }
    constexpr auto cend() const noexcept { return std::cend(_data); }
};

template<class First, class... Rest>
array(First, Rest...) -> array<typename detail::array_enforce_same<First, Rest...>::type, 1 + sizeof...(Rest)>;

template<typename T>
class vector {
    std::vector<T> data_;

public:
    vector() = default;
    explicit vector(const usize size)
      : data_(size.get()) {}
    explicit vector(std::vector<T> data)
      : data_(std::move(data)) {}

    [[nodiscard]] T& operator[](const usize idx) noexcept { return data_[idx.get()]; }
    [[nodiscard]] T operator[](const usize idx) const noexcept { return data_[idx.get()]; }
    [[nodiscard]] T* ptr(const usize idx) noexcept { return &data_[idx.get()]; }
    [[nodiscard]] const T* ptr(const usize idx) const noexcept { return &data_[idx.get()]; }
    [[nodiscard]] T& at(const usize idx) noexcept { return data_[idx.get()]; }
    [[nodiscard]] T at(const usize idx) const noexcept { return data_[idx.get()]; }
    [[nodiscard]] T* data() noexcept { return data_.data(); }
    [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    template<typename... Args>
    T& emplace_back(Args... args) { return data_.emplace_back(std::forward<Args>(args)...); }
    void push_back(T t) { data_.push_back(std::move(t)); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] usize size() const noexcept { return data_.size(); }

    auto erase(typename std::vector<T>::iterator pos) { return data_.erase(pos); }
    auto erase(typename std::vector<T>::const_iterator pos) { return data_.erase(pos); }
    auto erase(typename std::vector<T>::iterator first,
      typename std::vector<T>::iterator last) noexcept { return data_.erase(first, last); }
    auto erase(typename std::vector<T>::const_iterator first,
      typename std::vector<T>::const_iterator last) { return data_.erase(first, last); }
    void pop_back() noexcept { return data_.pop_back(); }

    void clear() noexcept { data_.clear(); }
    void resize(const usize new_size) { data_.resize(new_size.get()); }

    auto begin() noexcept { return data_.begin(); }
    auto end() noexcept { return data_.end(); }
    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }
    auto cbegin() const noexcept { return data_.cbegin(); }
    auto cend() const noexcept { return data_.cend(); }

    std::vector<T>& underlying_data() noexcept { return data_; }
    const std::vector<T>& underlying_data() const noexcept { return data_; }
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
