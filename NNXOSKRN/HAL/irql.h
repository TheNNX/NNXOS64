#ifndef NNX_IRQL_HEADER
#define NNX_IRQL_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
	typedef UINT8 KIRQL, *PKIRQL;

	KIRQL FASTCALL KfRaiseIrql(KIRQL newIrql);
	VOID FASTCALL KfLowerIrql(KIRQL newIrql);

	VOID NTAPI KeRaiseIrql(KIRQL newIrql, PKIRQL oldIrql);
	VOID NTAPI KeLowerIrql(KIRQL newIrql);

	KIRQL NTAPI KeGetCurrentIrql();

#define DISPATCH_LEVEL 32

#ifdef __cplusplus
}
#endif

#endif