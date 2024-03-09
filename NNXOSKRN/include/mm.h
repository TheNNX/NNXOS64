#ifndef NNX_MM_HEADER
#define NNX_MM_HEADER

#include "physical_allocator.h"
#include <object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MM_SECTION_FLAG_PAGED   1
#define MM_SECTION_FLAG_SHARED  2

/* Windows constants, nothing to do with the architecure dependent ones
 * from paging.h*/
#define PAGE_READONLY  0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08

    typedef struct _KMEMORY_SECTION
    {
        SIZE_T      NumberOfPages;
        PFN_NUMBER *Mappings;
        ULONG_PTR   Flags;
        HANDLE      SectionFile;
        ULONG       Protection;
    }KMEMORY_SECTION, *PKMEMORY_SECTION;

    typedef struct _SECTION_VIEW
    {
        LIST_ENTRY AddressSpaceEntry;
        HANDLE     hSection;
        ULONG_PTR  SectionOffset;
        ULONG_PTR  BaseAddress;
        ULONG_PTR  SizePages;
    }SECTION_VIEW, * PSECTION_VIEW;

#ifdef NNX_KERNEL
    NTSTATUS
    NTAPI
    MmInitObjects(VOID);

    NTSTATUS
    NTAPI
    MmHandleViewPageFault(
        PSECTION_VIEW View,
        ULONG_PTR Address,
        BOOLEAN Write);

    NTSTATUS
    NTAPI
    MmHandlePageFault(
        ULONG_PTR Address,
        BOOLEAN Write);
#endif

    NTSYSAPI
    NTSTATUS
    NTAPI
    NtCreateSectionFromFile(
        PHANDLE pOutHandle,
        PUNICODE_STRING pFileName);

    NTSTATUS
    NTAPI
    MmCreateView(
        PADDRESS_SPACE AddressSpace,
        HANDLE hSection,
        ULONG_PTR VirtualAddress,
        ULONG_PTR Offset,
        SIZE_T Size,
        PSECTION_VIEW* pOutView);

#ifdef __cplusplus
}
#endif

#endif