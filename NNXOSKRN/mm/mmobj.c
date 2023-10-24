/** 
 * mmobj.c - The memory manager object interface.
 * This file defines the object types used by the memory manager.
 */

#include <mm.h>
#include <object.h>
#include <pool.h>
#include <ntdebug.h>
#include <scheduler.h>

typedef struct _SECTION_PROCESS_LINK
{
    LIST_ENTRY       AddressSpaceEntry;
    LIST_ENTRY       SectionEntry;
    PADDRESS_SPACE   AddressSpace;
    PKMEMORY_SECTION Section;
}SECTION_PROCESS_LINK, *PSECTION_PROCESS_LINK;

static KSPIN_LOCK     MmSectionGlobalLock;
static POBJECT_TYPE   MmSectionType;
static UNICODE_STRING MmSectionTypeName = RTL_CONSTANT_STRING(L"MemorySection");

typedef struct _MEMORY_SECTION_CREATION_DATA
{
    SIZE_T    NumberOfPages;
    ULONG_PTR BaseVirtualAddress;
    ULONG_PTR Flags;
}MEMORY_SECTION_CREATION_DATA, *PMEMORY_SECTION_CREATION_DATA;

static
NTSTATUS
NTAPI
MiCreateSection(
    PKMEMORY_SECTION Section,
    PMEMORY_SECTION_CREATION_DATA Data)
{
    if (Data == NULL || Data->NumberOfPages == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }
    Section->NumberOfPages = Data->NumberOfPages;
    Section->BaseVirtualAddress = Data->BaseVirtualAddress;
    Section->Flags = Data->Flags;
    KeInitializeSpinLock(&Section->SectionLock);

    /* FIXME: Potential non paged pool memory hog. */
    Section->Mappings = ExAllocatePoolZero(
        NonPagedPool,
        Section->NumberOfPages * sizeof(ULONG_PTR),
        'MSEC');
    if (Section->Mappings == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
MiDeleteSection(
    PKMEMORY_SECTION Section)
{
    ASSERT(Section != NULL && Section->Mappings != NULL);

    ExFreePoolWithTag(Section->Mappings, 'MSEC');

    Section->NumberOfPages = 0;
    Section->Mappings = NULL;
    Section->BaseVirtualAddress = NULL;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
MiMapSection(
    PADDRESS_SPACE AddressSpace,
    PKMEMORY_SECTION Section)
{
    SIZE_T Index;
    ULONG_PTR Address;
    ULONG_PTR VirtualAddress = Section->BaseVirtualAddress;
    NTSTATUS Status;

    KeSetCustomThreadAddressSpace(KeGetCurrentThread(), AddressSpace);

    /* Check if the section can be mapped into this address space at this
     * virtual address. */
    for (Index = 0; Index < Section->NumberOfPages; Index++)
    {
        Address = VirtualAddress + Index * PAGE_SIZE;
        if (PagingGetTableMapping(Address) != 0)
        {
            return STATUS_CONFLICTING_ADDRESSES;
        }
    }

    /* No process is using the section yet - initialize it.
       It is neccessary, beacause there aren't any preexisting mapping that can
       be used yet. */
    if (IsListEmpty(&Section->ProcessLinkHead))
    {
        for (Index = 0; Index < Section->NumberOfPages; Index++)
        {
            Status = MmAllocatePfn(&Section->Mappings[Index]);
            if (!NT_SUCCESS(Status))
            {
                SIZE_T Jndex;
                for (Jndex = 0; Jndex < Index; Jndex++)
                {
                    /* Ignore the status - nothing can be done if this fails. */
                    MmFreePfn(Section->Mappings[Jndex]);
                }

                KeClearCustomThreadAddressSpace(KeGetCurrentThread());
                return Status;
            }
        }
    }

    /* Actually map the section pages into the address space. */
    for (Index = 0; Index < Section->NumberOfPages; Index++)
    {
        Address = VirtualAddress + Index * PAGE_SIZE;
        Status = PagingMapPage(
            Address, 
            PA_FROM_PFN(Section->Mappings[Index]), 
            PAGING_USER_SPACE);
        if (!NT_SUCCESS(Status))
        {
            /* Unmap all previously mapped pages of the section if an error
               occured while mapping a page. */
            SIZE_T Jndex;
            for (Jndex = 0; Jndex < Index; Jndex++)
            {
                /* Ignore the status - nothing can be done if this fails. */
                PagingMapPage(VirtualAddress + Jndex * PAGE_SIZE, 0, 0);
            }

            KeClearCustomThreadAddressSpace(KeGetCurrentThread());
            return Status;
        }
    }

    KeClearCustomThreadAddressSpace(KeGetCurrentThread());
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
MmAttachSectionToProcess(
    PADDRESS_SPACE AddressSpace,
    PKMEMORY_SECTION Section)
{
    KIRQL Irql;
    PSECTION_PROCESS_LINK Link;
    NTSTATUS Status;
    
    /* The MmSectionGlobalLock is used to "atomize" the double lock
       acquisition, so it is possible to lock section locks without any address
       space locking. */
    KeAcquireSpinLock(&MmSectionGlobalLock, &Irql);
    KeAcquireSpinLockAtDpcLevel(&AddressSpace->Lock);
    KeAcquireSpinLockAtDpcLevel(&Section->SectionLock);
    KeReleaseSpinLockFromDpcLevel(&MmSectionGlobalLock);

    Status = STATUS_SUCCESS;

    ObReferenceObject(Section);
    Link = ExAllocatePoolWithTag(NonPagedPool, sizeof(*Link), 'MSEL');
    if (Link == NULL)
    {
        Status = STATUS_NO_MEMORY;
        goto Exit;
    }

    Status = MiMapSection(AddressSpace, Section);
    if (!NT_SUCCESS(Status))
    {
        goto Exit;
    }

    Link->AddressSpace = AddressSpace;
    Link->Section = Section;
    InsertTailList(&Section->ProcessLinkHead, &Link->SectionEntry);
    InsertTailList(&AddressSpace->SectionLinkHead, &Link->AddressSpaceEntry);
Exit:
    KeAcquireSpinLockAtDpcLevel(&MmSectionGlobalLock);
    KeReleaseSpinLockFromDpcLevel(&Section->SectionLock);
    KeReleaseSpinLockFromDpcLevel(&AddressSpace->Lock);
    KeReleaseSpinLock(&MmSectionGlobalLock, Irql);
    return Status;
}

NTSTATUS
NTAPI
MmDeteachSectionFromProcess(
    PADDRESS_SPACE AddressSpace,
    PKMEMORY_SECTION Section)
{
    KIRQL Irql;
    PSECTION_PROCESS_LINK Link, CurrentLink;
    PLIST_ENTRY Current, Head;
    NTSTATUS Status;

    KeAcquireSpinLock(&MmSectionGlobalLock, &Irql);
    KeAcquireSpinLockAtDpcLevel(&AddressSpace->Lock);
    KeAcquireSpinLockAtDpcLevel(&Section->SectionLock);
    KeReleaseSpinLockFromDpcLevel(&MmSectionGlobalLock);

    Status = STATUS_SUCCESS;

    /* Find the link between the address space and the section and delete it. 
       */
    Current = AddressSpace->SectionLinkHead.First;
    Head = &AddressSpace->SectionLinkHead;
    Link = NULL;

    while (Current != Head)
    {
        CurrentLink = CONTAINING_RECORD(
            Current, 
            SECTION_PROCESS_LINK, 
            AddressSpaceEntry);

        if (CurrentLink->Section == Section)
        {
            Link = CurrentLink;
        }

        Current = Current->Next;
    }

    if (Link == NULL)
    {
        /* TODO: Better status for section not being mapped into the given 
           address space. */
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    RemoveEntryList(&Link->SectionEntry);
    RemoveEntryList(&Link->AddressSpaceEntry);
    ExFreePool(Link);
    ObDereferenceObject(Section);
Exit:
    KeAcquireSpinLockAtDpcLevel(&MmSectionGlobalLock);
    KeReleaseSpinLockFromDpcLevel(&Section->SectionLock);
    KeReleaseSpinLockFromDpcLevel(&AddressSpace->Lock);
    KeReleaseSpinLock(&MmSectionGlobalLock, Irql);
    return Status;
}

NTSTATUS
NTAPI
MmInitObjects()
{
    NTSTATUS Status;

    KeInitializeSpinLock(&MmSectionGlobalLock);

    Status = ObCreateType(
        &MmSectionType,
        &MmSectionTypeName,
        sizeof(KMEMORY_SECTION));
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    MmSectionType->OnDelete = MiDeleteSection;
    MmSectionType->OnCreate = MiCreateSection;

    return Status;
}

NTSTATUS
NTAPI
MiCreateSectionObject(
    ULONG_PTR BaseVa,
    SIZE_T NumberOfPages,
    ULONG_PTR Flags,
    PKMEMORY_SECTION *outSection)
{
    NTSTATUS Status;
    MEMORY_SECTION_CREATION_DATA Data;
    OBJECT_ATTRIBUTES Attributes;

    InitializeObjectAttributes(&Attributes, NULL, 0, NULL, NULL);

    Data.BaseVirtualAddress = BaseVa;
    Data.NumberOfPages = NumberOfPages;
    Data.Flags = Flags;

    Status = ObCreateObject(
        (PVOID*)&outSection,
        0, 
        KernelMode, 
        &Attributes,
        MmSectionType, 
        &Data);

    return Status;
}

