#ifndef GAMEBOIADVANCE_MACROS_H
#define GAMEBOIADVANCE_MACROS_H

#include <spdlog/spdlog.h>

#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__) /*NOLINT*/
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__) /*NOLINT*/
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__) /*NOLINT*/
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__) /*NOLINT*/
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__) /*NOLINT*/
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__) /*NOLINT*/

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__);      \
        __assume(0);                                                            \
    } while(0)
#elif defined(__GNUC__) && __GNUC__ > 3
  // Clang also defines __GNUC__ (as 4)
  #define FORCEINLINE inline __attribute__ ((__always_inline__))
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__);      \
        __builtin_unreachable();                                                \
    } while(0)
#else
  #define FORCEINLINE inline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__);      \
        std::terminate();                                                       \
    } while(0)
#endif

#endif //GAMEBOIADVANCE_MACROS_H
