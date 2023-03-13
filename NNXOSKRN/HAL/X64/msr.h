#ifndef NNX_REGISTERS_HEADER
#define NNX_REGISTERS_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <intrin.h>

#define IA32_STAR			0xC0000081UL
#define IA32_LSTAR			0xC0000082UL
#define IA32_FMASK			0xC0000084UL
#define IA32_KERNEL_GS_BASE 0xC0000102UL
#define IA32_GS_BASE		0xC0000101UL
#define IA32_EFER			0xC0000080UL

#ifdef __cplusplus
}
#endif

#endif