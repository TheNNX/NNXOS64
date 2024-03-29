#pragma once
#include <nnxtype.h>
#include <bugcheck.h>
#include <cpu.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG
#define ASSERT(expr) (!(expr) ? \
    __debugbreak() \
    : ((VOID)0))

#define ASSERTMSG(expr, ...) \
    if (!(expr))\
    {\
        PrintT(__VA_ARGS__); __debugbreak();\
    }

#else
#define ASSERT(expr) ((VOID)0)
#endif

#ifdef __cplusplus
}
#endif