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
#define PAGE_READONLY  0x02L
#define PAGE_READWRITE 0x04L
#define PAGE_WRITECOPY 0x08L

    typedef struct _KMEMORY_SECTION
    {
        SIZE_T      NumberOfPages;
        PFN_NUMBER *Mappings;
        ULONG_PTR   Flags;
        HANDLE      SectionFile;
        KSPIN_LOCK  Lock;
    }KMEMORY_SECTION, *PKMEMORY_SECTION;

    typedef struct _SECTION_VIEW
    {
        LIST_ENTRY AddressSpaceEntry;
        HANDLE     hSection;
        ULONG_PTR  FileOffset;
        ULONG_PTR  MappingStart;
        ULONG_PTR  BaseAddress;
        ULONG_PTR  SizePages;
        PUCHAR     Name;
        ULONG      Protection;
    }SECTION_VIEW, *PSECTION_VIEW;

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
    NtReferenceSectionFromFile(
        PHANDLE pOutHandle,
        PUNICODE_STRING pFileName);

    NTSTATUS
    NTAPI
    MmCreateView(
        PADDRESS_SPACE AddressSpace,
        HANDLE hSection,
        ULONG_PTR VirtualAddress,
        ULONG_PTR MappingStart,
        ULONG_PTR Offset,
        ULONG Protection,
        SIZE_T Size,
        PSECTION_VIEW* pOutView);

    NTSTATUS
    NTAPI
    MmDeleteView(
        PADDRESS_SPACE AddressSpace,
        PSECTION_VIEW View);

#ifdef __cplusplus
}
#endif

#endif