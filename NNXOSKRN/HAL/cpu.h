#ifndef NNX_CPU_HEADER
#define NNX_CPU_HEADER

#include <nnxtype.h>

typedef CHAR	KPROCESSOR_MODE;

#define UserMode 1
#define KernelMode 0

#define KAFFINITY_ALL 0xFFFFFFFFFFFFFFFFULL
typedef ULONG_PTR	KAFFINITY;

#ifdef __cplusplus
extern "C" {
#endif

    extern UINT KeNumberOfProcessors;

    ULONG 
    NTAPI
    KeGetCurrentProcessorId();

    ULONG_PTR
    NTAPI
    HalpGetCurrentAddress();

    VOID
    NTAPI
    KeSendIpi(
        KAFFINITY TargetCpus, 
        BYTE Vector);


#ifdef __cplusplus
}
#endif


#endif
