#ifndef NNX_MDL_HEADER
#define NNX_MDL_HEADER

#include <nnxtype.h>
#include <scheduler.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _MDL
    {
        struct _MDL* Next;
        SHORT Size;
        SHORT MdlFlags;
        PEPROCESS Process;
        PVOID MappedSystemVa;
        PVOID StartVa;
        ULONG ByteCount;
        ULONG ByteOffset;
    }MDL, *PMDL;

    VOID
        MmInitializeMdl(
            PMDL Buffer,
            PVOID BaseVa,
            SIZE_T Length
        );

#ifdef __cplusplus
}
#endif

#endif