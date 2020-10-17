#ifndef GAMEBOIADVANCE_MACROS_H
#define GAMEBOIADVANCE_MACROS_H

#include <spdlog/spdlog.h>

#define LOG_CRITICAL(fmt, ...) SPDLOG_CRITICAL(fmt, __VA_ARGS__) /*NOLINT*/
#define LOG_ERROR(fmt, ...) SPDLOG_ERROR(fmt, __VA_ARGS__) /*NOLINT*/
#define LOG_WARN(fmt, ...) SPDLOG_WARN(fmt, __VA_ARGS__) /*NOLINT*/
#define LOG_INFO(fmt, ...) SPDLOG_INFO(fmt, __VA_ARGS__) /*NOLINT*/
#define LOG_DEBUG(fmt, ...) SPDLOG_DEBUG(fmt, __VA_ARGS__) /*NOLINT*/
#define LOG_TRACE(fmt, ...) SPDLOG_TRACE(fmt, __VA_ARGS__) /*NOLINT*/

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
