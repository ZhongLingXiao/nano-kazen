#pragma once

#define KAZEN_VERSION_MAJOR 0
#define KAZEN_VERSION_MINOR 1
#define KAZEN_VERSION_PATCH 0

#define KAZEN_AUTHORS "ZhongLingXiao && Joey Chen"
#define KAZEN_YEAR 2021

/// defines
#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif