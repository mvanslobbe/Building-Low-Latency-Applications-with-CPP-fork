#pragma once

#include <cstring>
#include <iostream>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define ASSERT(cond, msg)                                               \
  do {                                                                  \
    if (UNLIKELY(!(cond)))                                              \
      [&]() __attribute__((cold, noinline, noreturn)) {                 \
        std::cerr << "ASSERT : " << (msg) << std::endl;                 \
        exit(EXIT_FAILURE);                                             \
      }();                                                              \
  } while (false)

inline auto FATAL(const std::string &msg) noexcept {
  std::cerr << "FATAL : " << msg << std::endl;

  exit(EXIT_FAILURE);
}
