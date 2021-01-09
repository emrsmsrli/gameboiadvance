/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_MACROS_H
#define GAMEBOIADVANCE_MACROS_H

#include <spdlog/spdlog.h>

#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__) /*NOLINT*/
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__) /*NOLINT*/
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__) /*NOLINT*/
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__) /*NOLINT*/
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__) /*NOLINT*/
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__) /*NOLINT*/

#if DEBUG
  #define ASSERT(x) do { if(!(x)) { LOG_ERROR("assertion failure: " # x); std::terminate(); } } while(0)
#else
  #define ASSERT(x)
#endif

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit");                                   \
        __assume(0);                                                            \
    } while(0)
  #define LIKELY(x) (x)
  #define UNLIKELY(x) (x)
#elif defined(__GNUC__) && __GNUC__ > 3
  // Clang also defines __GNUC__ (as 4)
  #define FORCEINLINE inline __attribute__ ((__always_inline__))
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit");                                   \
        __builtin_unreachable();                                                \
    } while(0)
  #define LIKELY(x) __builtin_expect(!!(x),1)
  #define UNLIKELY(x) __builtin_expect(!!(x),0)
#else
  #define FORCEINLINE inline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL("unreachable code hit");                                   \
        std::terminate();                                                       \
    } while(0)
  #define LIKELY(x) (x)
  #define UNLIKELY(x) (x)
#endif

#endif //GAMEBOIADVANCE_MACROS_H
