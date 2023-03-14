#ifndef NNX_HALTEST_HEADER
#define NNX_HALTEST_HEADER

#include <nnxtype.h>

#ifdef NNX_HAL
#define HALDLL __declspec(dllexport)
#else
#define HALDLL __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

    HALDLL
    NTSTATUS
    NTAPI
    HalInit();

#ifdef __cplusplus
}
#endif

#endif