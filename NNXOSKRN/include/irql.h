#ifndef NNX_IRQL_HEADER
#define NNX_IRQL_HEADER

#ifdef __cplusplus
extern "C" {
#endif
#include <nnxtype.h>
    typedef UINT8 KIRQL, *PKIRQL;

    NTSYSAPI
    KIRQL
    FASTCALL 
    KfRaiseIrql(
        KIRQL newIrql);

    NTSYSAPI
    VOID 
    FASTCALL 
    KfLowerIrql(
        KIRQL oldIrql);

    NTSYSAPI
    VOID 
    NTAPI 
    KeRaiseIrql(
        KIRQL newIrql, 
        PKIRQL pOldIrql);

    NTSYSAPI
    VOID 
    NTAPI KeLowerIrql(
        KIRQL oldIrql);

    NTSYSAPI
    KIRQL 
    NTAPI 
    KeGetCurrentIrql();


#define HIGH_LEVEL         31
#define IPI_LEVEL          14
#define CLOCK_LEVEL        13
/* TODO: When (or if) the scheduling is moved to a DPC, SYNCH_LEVEL should be 
 * changed to 12. Setting it to 13 is a temporary hack, so the dispatcher lock
 * can be acquired at CLOCK_LEVEL (because going to SYNCH_LEVEL doesn't lower
 * the IRQL, and KeRaiseIrql in KiAcquireDispatcherLock successds) */
#define SYNCH_LEVEL        13
#define DISPATCH_LEVEL     2
#define APC_LEVEL          1
#define PASSIVE_LEVEL      0

#define DPC_LEVEL DISPATCH_LEVEL

#ifdef __cplusplus
}
#endif

#endif