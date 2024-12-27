#pragma once
#include <nnxtype.h>
#include <bugcheck.h>
#include <cpu.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG
#define ASSERTBRK(expr) (!(expr) ? \
    __debugbreak() \
    : ((VOID)0))

#define ASSERTMSG(expr, ...) \
    if (!(expr))\
    {\
        void PrintTA(const char* c, ...);\
        PrintTA(__VA_ARGS__); __debugbreak();\
    }

#define ASSERT(expr) do {ASSERTMSG(expr, "Assertion failed at %s:%i, " # expr " was false.\n", __FILE__, __LINE__);}while(0)

#else
#define ASSERT(expr) ((VOID)0)
#endif

#ifdef __cplusplus
}
#endif