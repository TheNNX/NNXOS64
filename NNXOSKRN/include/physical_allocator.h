#ifndef NNX_PHYSALLOC_HEADER
#define NNX_PHYSALLOC_HEADER
#include <nnxtype.h>
#include <ntlist.h>

#ifdef __cplusplus
extern "C"
{
#endif
    typedef ULONG_PTR PFN_NUMBER;

    typedef struct _MMPFN_LIST
    {
        LIST_ENTRY  PfnListHead;
        SIZE_T      NumberOfPfns;
    }MMPFN_LIST, *PMMPFN_LIST;

    typedef struct _MMPFN_ENTRY
    {
        LIST_ENTRY  ListEntry;
        PMMPFN_LIST InList;
        ULONG_PTR   Flags;
    }MMPFN_ENTRY, *PMMPFN_ENTRY;

#define MMPFN_FLAG_NO_PAGEOUT 1
#define MMPFN_FLAG_PERMAMENT  2

#define PFN_FROM_PA(x) ((PFN_NUMBER)(x / PAGE_SIZE))
#define PA_FROM_PFN(x) ((ULONG_PTR)(x * PAGE_SIZE))

#ifdef NNX_KERNEL
    VOID 
    NTAPI
    MmReinitPhysAllocator(
        PMMPFN_ENTRY pfnEntries,
        SIZE_T numberOfPfnEntries);

    NTSTATUS 
    NTAPI
    MmAllocatePfn(
        PFN_NUMBER* pPfnNumber);

    NTSTATUS 
    NTAPI
    MmFreePfn(
        PFN_NUMBER pfnNumber);

    NTSTATUS
    NTAPI
    MmAllocatePhysicalAddress(
        ULONG_PTR* pPhysAddress);

    NTSTATUS 
    NTAPI
    MmFreePhysicalAddress(
        ULONG_PTR physAddress);

    NTSTATUS 
    NTAPI
    MmMarkPfnAsUsed(
        PFN_NUMBER pfnNumber);

    VOID 
    NTAPI
    MiFlagPfnsForRemap();

    VOID 
    DrawMap();

    extern PMMPFN_ENTRY PfnEntries;
    extern SIZE_T       NumberOfPfnEntries;
#endif

#ifdef __cplusplus
}
#endif

#endif