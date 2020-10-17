#ifndef GAMEBOIADVANCE_MACROS_H
#define GAMEBOIADVANCE_MACROS_H

#include <spdlog/spdlog.h>

#if defined(_MSC_VER)
  #define FORCEINLINE __forceinline
  #define UNREACHABLE()                                                                     \
    do {                                                                                    \
        SPDLOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__); /*NOLINT*/    \
        __assume(0);                                                                        \
    } while(0)
#elif defined(__GNUC__) && __GNUC__ > 3
  // Clang also defines __GNUC__ (as 4)
  #define FORCEINLINE inline __attribute__ ((__always_inline__))
  #define UNREACHABLE()                                                                     \
    do {                                                                                    \
        SPDLOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__); /*NOLINT*/    \
        __builtin_unreachable();                                                            \
    } while(0)
#else
  #define FORCEINLINE inline
  #define UNREACHABLE()                                                                     \
    do {                                                                                    \
        SPDLOG_CRITICAL("unreachable code hit at {}:{}", __FILE__, __LINE__); /*NOLINT*/    \
        std::terminate();                                                                   \
    } while(0)
#endif

#endif //GAMEBOIADVANCE_MACROS_H
