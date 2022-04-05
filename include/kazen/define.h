#pragma once

#define KAZEN_VERSION_MAJOR 0
#define KAZEN_VERSION_MINOR 1
#define KAZEN_VERSION_PATCH 0

#define KAZEN_AUTHORS "ZhongLingXiao && Joey Chen"
#define KAZEN_YEAR 2022

/// defines
#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#define KAZEN_INLINE __attribute__((always_inline)) inline
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)