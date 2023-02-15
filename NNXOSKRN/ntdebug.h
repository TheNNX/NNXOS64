#pragma once
#include <nnxtype.h>
#include <bugcheck.h>
#include <HAL/cpu.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define ASSERT(expr) (!(expr) ? \
    (KeBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, 0x80000003, HalpGetCurrentAddress(), __LINE__, 0)) \
    : ((VOID)0))
#else
#define ASSERT(expr) ((VOID)0)
#endif

#ifdef __cplusplus
}
#endif