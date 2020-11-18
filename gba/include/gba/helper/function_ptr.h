#ifndef GAMEBOIADVANCE_FUNCTION_PTR_H
#define GAMEBOIADVANCE_FUNCTION_PTR_H

#include <functional>

namespace gba {

template<typename>
struct function_ptr;

template<typename Ret, typename... Args>
struct function_ptr<Ret(Args...)> {
    using type = Ret(Args...);

    type* ptr = nullptr;

    template<typename... T>
    Ret operator()(T&&... args) const noexcept { return std::invoke(ptr, std::forward<T>(args)...); }

    [[nodiscard]] bool is_valid() const noexcept { return ptr != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }
};

template<typename Ret, typename... Args>
function_ptr(Ret(*)(Args...)) noexcept -> function_ptr<Ret(Args...)>;

} // namespace gba

#endif //GAMEBOIADVANCE_FUNCTION_PTR_H
