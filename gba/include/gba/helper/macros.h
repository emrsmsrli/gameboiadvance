/*
 * Copyright (C) 2020  emrsmsrli
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#ifndef GAMEBOIADVANCE_MACROS_H
#define GAMEBOIADVANCE_MACROS_H

#include <spdlog/spdlog.h>

#define LOG_CRITICAL(category, ...) SPDLOG_CRITICAL("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define LOG_ERROR(category, ...) SPDLOG_ERROR("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define LOG_WARN(category, ...) SPDLOG_WARN("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define LOG_INFO(category, ...) SPDLOG_INFO("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define LOG_DEBUG(category, ...) SPDLOG_DEBUG("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define LOG_TRACE(category, ...) SPDLOG_TRACE("[" #category "] " __VA_ARGS__) /*NOLINT*/
#define PANIC()                           \
  do {                                    \
      spdlog::default_logger()->flush();  \
      std::terminate();                   \
  } while(0)

#if DEBUG || defined(ENABLE_ASSERTIONS)
  #define ASSERT(x) do { if(!(x)) { LOG_ERROR(assert, "assertion failure: " # x); PANIC(); } } while(0)
#else
  #define ASSERT(x) (void)0
#endif

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL(assert, "unreachable code hit");                           \
        __assume(0);                                                            \
    } while(0)
  #define LIKELY(x) (x)
  #define UNLIKELY(x) (x)
#elif defined(__GNUC__) && __GNUC__ > 3
  // Clang also defines __GNUC__ (as 4)
  #define FORCEINLINE inline __attribute__ ((__always_inline__))
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL(assert, "unreachable code hit");                           \
        __builtin_unreachable();                                                \
    } while(0)
  #define LIKELY(x) __builtin_expect(!!(x),1)
  #define UNLIKELY(x) __builtin_expect(!!(x),0)
#else
  #define FORCEINLINE inline
  #define UNREACHABLE()                                                         \
    do {                                                                        \
        LOG_CRITICAL(assert, "unreachable code hit");                           \
        PANIC();                                                       \
    } while(0)
  #define LIKELY(x) (x)
  #define UNLIKELY(x) (x)
#endif

#endif //GAMEBOIADVANCE_MACROS_H
