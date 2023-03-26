#ifndef NNX_CPU_HEADER
#define NNX_CPU_HEADER

#include <nnxtype.h>
#include <intrin.h>

typedef enum _MODE
{
    KernelMode = 0,
    UserMode = 1
}MODE;

typedef CHAR	KPROCESSOR_MODE;

#define KAFFINITY_ALL 0xFFFFFFFFFFFFFFFFULL
typedef ULONG_PTR	KAFFINITY;

#ifdef __cplusplus
extern "C" {
#endif

    NTSYSAPI
    ULONG
    NTAPI
    KeGetCurrentProcessorId();

#ifdef NNX_KERNEL
    extern UINT KeNumberOfProcessors;

    ULONG_PTR
    NTAPI
    HalpGetCurrentAddress();

    VOID
    NTAPI
    KeSendIpi(
        KAFFINITY TargetCpus,
        BYTE Vector);

    VOID
    NTAPI
    HalSetPcr(
        struct _KPCR* pcr);

#ifdef _M_AMD64
#define HalSetTpr __writecr8
#define HalGetTpr __readcr8
#endif

#endif

#ifdef __cplusplus
}
#endif


#endif
