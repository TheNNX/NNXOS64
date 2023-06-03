#include "mdl.h"
#include <pool.h>
#include <ntdebug.h>
#include <physical_allocator.h>

VOID
NTAPI
MmInitializeMdl(
    PMDL pMdl,
    PVOID BaseVa,
    SIZE_T Length)
{
    ASSERT(pMdl != NULL);
    pMdl->ByteCount = (ULONG)Length;
    pMdl->StartVa = BaseVa;
    pMdl->MappedSystemVa = NULL;
    pMdl->ByteOffset = ((ULONG_PTR)BaseVa) & PAGE_FLAGS_MASK;
    pMdl->MdlFlags = 0;
}

VOID
NTAPI
MmBuildMdlForNonPagedPool(
    PMDL pMdl)
{
    PFN_NUMBER* FrameDescriptor;
    ULONG_PTR CurrentAddress;

    ASSERT(pMdl != NULL);
    FrameDescriptor = (PFN_NUMBER*)((ULONG_PTR)pMdl + sizeof(*pMdl));

    for (CurrentAddress = (ULONG_PTR)pMdl->StartVa;
         CurrentAddress < (ULONG_PTR)pMdl->StartVa + pMdl->Size;
         CurrentAddress += PAGE_SIZE)
    {
        ULONG_PTR Index;
        ULONG_PTR Aligned = PAGE_ALIGN(CurrentAddress);
        Index = (Aligned - PAGE_ALIGN(pMdl->StartVa)) / PAGE_SIZE;

        FrameDescriptor[Index] = PAGE_ALIGN(PagingGetTableMapping(Aligned));
    }

    pMdl->MappedSystemVa = pMdl->StartVa;
    pMdl->Process = CONTAINING_RECORD(
        KeGetCurrentThread()->Process, 
        EPROCESS, 
        Pcb);
}