#ifndef NNX_CPU_HEADER
#define NNX_CPU_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef CHAR	KPROCESSOR_MODE;

#define KAFFINITY_ALL 0xFFFFFFFFFFFFFFFFULL
	typedef UINT64	KAFFINITY;

	typedef enum _MODE
	{
		KernelMode = 0,
		UserMode = 1
	}MODE;

	VOID HalSetPcr(struct _KPCR* pcr);
	struct _KPCR* HalSwapInPcr();

#ifdef __cplusplus
}
#endif

#endif