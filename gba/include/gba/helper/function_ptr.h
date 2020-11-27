#ifndef GAMEBOIADVANCE_FUNCTION_PTR_H
#define GAMEBOIADVANCE_FUNCTION_PTR_H

#include <functional>

namespace gba {

/**
 * Basic impl function_ptr. Not defined on purpose.
 * More lightweight than <code>gba::delegate</code> since it also captures a payload instance.
 */
template<typename, typename=void>
struct function_ptr;

/**
 * This specialization of function_ptr is for free functions.
 * It has the same functionality as using a type alias
 *
 * @tparam Ret Return type of the function
 * @tparam Args Types of the arguments of the function
 */
template<typename Ret, typename... Args>
struct function_ptr<Ret(Args...)> {
    using type = Ret(Args...);

    type* ptr = nullptr;

    /**
     * This function delegates the invocation of the underlying function ptr
     * @tparam T Types of the arguments of the function
     * @param args Arguments
     * @return Return value from the function
     */
    template<typename... T>
    constexpr Ret operator()(T&&... args) const noexcept { return std::invoke(ptr, std::forward<T>(args)...); }

    /** @return if this function_ptr is bound or not */
    [[nodiscard]] constexpr bool is_valid() const noexcept { return ptr != nullptr; }
    /** @return if this function_ptr is bound or not */
    explicit constexpr operator bool() const noexcept { return is_valid(); }
};

/**
 * This specialization of function_ptr is for member functions.
 * It has the same functionality as using a type alias
 *
 * @tparam Ret Return type of the function
 * @tparam Args Types of the arguments of the function
 */
template<typename Class, typename Ret, typename... Args>
struct function_ptr<Class, Ret(Args...)> {
    using type = Ret (Class::*)(Args...);

    type ptr = nullptr;

    /**
     * This function delegates the invocation of the underlying function ptr
     * @tparam T Types of the arguments of the function
     * @param args Arguments. Must contain the instance of the class if the function is non-static.
     * @return Return value from the function
     */
    template<typename... T>
    constexpr Ret operator()(T&&... args) const noexcept { return std::invoke(ptr, std::forward<T>(args)...); }

    /** @return if this function_ptr is bound or not */
    [[nodiscard]] constexpr bool is_valid() const noexcept { return ptr != nullptr; }
    /** @return if this function_ptr is bound or not */
    explicit constexpr operator bool() const noexcept { return is_valid(); }
};

/** CTAD guide for free functions */
template<typename Ret, typename... Args>
function_ptr(Ret(*)(Args...)) noexcept -> function_ptr<Ret(Args...)>;

/** CTAD guide for member functions */
template<typename Class, typename Ret, typename... Args>
function_ptr(Ret(Class::*)(Args...)) noexcept -> function_ptr<Class, Ret(Args...)>;

} // namespace gba

#endif //GAMEBOIADVANCE_FUNCTION_PTR_H
