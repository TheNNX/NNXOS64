#ifndef NNX_MM_HEADER
#define NNX_MM_HEADER

#include "physical_allocator.h"
#include <object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM_SECTION_FLAG_PAGED   1
#define MM_SECTION_FLAG_SHARED  2

    typedef struct _KMEMORY_SECTION
    {
        ULONG_PTR   BaseVirtualAddress;
        SIZE_T      NumberOfPages;
        PFN_NUMBER *Mappings;
        ULONG_PTR   Flags;
        LIST_ENTRY  ProcessLinkHead;
        KSPIN_LOCK  SectionLock;
        HANDLE      SectionFile;
    }KMEMORY_SECTION, *PKMEMORY_SECTION;

#ifdef NNX_KERNEL
    NTSTATUS
    NTAPI
    MmInitObjects(VOID);

    NTSTATUS
    NTAPI
    MmHandleSectionPageFault(
        PKMEMORY_SECTION pMemorySection,
        ULONG_PTR Address);

    NTSTATUS
    NTAPI
    MmHandlePageFault(
        ULONG_PTR Address);
#endif

#ifdef __cplusplus
}
#endif

#endif