/** 
 * mmobj.c - The memory manager object interface.
 * This file defines the object types used by the memory manager.
 */

#include <SimpleTextIO.h>
#include <mm.h>
#include <object.h>
#include <pool.h>
#include <ntdebug.h>
#include <scheduler.h>
#include <file.h>
#include <rtl.h>

UINT16
PagingFlagsFromSectionFlags(
    PFN_NUMBER Mapping,
    PKMEMORY_SECTION Section,
    PSECTION_VIEW View);


VOID MiNotifyAboutSectionChanges()
{
    /* TODO: Notify other cores about changes in a section/address space, so
     * they can update their address spaces accordingly. */
}

static POBJECT_TYPE   MmSectionType;
static UNICODE_STRING MmSectionTypeName = RTL_CONSTANT_STRING(L"MemorySection");

typedef struct _MEMORY_SECTION_CREATION_DATA
{
    SIZE_T    NumberOfPages;
    ULONG_PTR Flags;
    HANDLE    File;
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
    Section->Flags = Data->Flags;
    Section->SectionFile = Data->File;
    KeInitializeSpinLock(&Section->Lock);

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

NTSTATUS
NTAPI
MiResizeSection(
    PKMEMORY_SECTION Section,
    SIZE_T NumPages)
{
    PFN_NUMBER* oldMappings;
    SIZE_T oldSize;

    ASSERT(Section->Lock != 0);

    oldSize = Section->NumberOfPages;
    oldMappings = Section->Mappings;

    Section->NumberOfPages = NumPages;
    Section->Mappings = ExAllocatePoolZero(
        NonPagedPool,
        Section->NumberOfPages * sizeof(*Section->Mappings),
        'MSEC');
    if (Section->Mappings == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    RtlCopyMemory(
        Section->Mappings,
        oldMappings, 
        oldSize * sizeof(*Section->Mappings));
    
    ExFreePool(oldMappings);
    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
MiDeleteSection(
    PKMEMORY_SECTION Section)
{
    ASSERT(Section != NULL && Section->Mappings != NULL);

    if (Section->SectionFile != NULL)
    {
        ObCloseHandle(Section->SectionFile, KernelMode);
    }

    ExFreePoolWithTag(Section->Mappings, 'MSEC');

    Section->NumberOfPages = 0;
    Section->Mappings = NULL;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
MiMapView(
    PADDRESS_SPACE AddressSpace,
    PSECTION_VIEW SectionView)
{
    SIZE_T Index, MappingIndex;
    ULONG_PTR Address;
    PKMEMORY_SECTION Section;
    ULONG_PTR VirtualAddress;
    NTSTATUS Status;

    Status = ObReferenceObjectByHandle(
        SectionView->hSection,
        0,
        NULL,
        KernelMode,
        &Section,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    KeSetCustomThreadAddressSpace(KeGetCurrentThread(), AddressSpace);
    ASSERT(Section->Lock != 0);
    VirtualAddress = SectionView->BaseAddress;

    /* Check if the section can be mapped into this address space at this
     * virtual address. */
    for (Index = 0; Index < SectionView->SizePages; Index++)
    {
        Address = VirtualAddress + Index * PAGE_SIZE;
        MappingIndex = (Address - SectionView->BaseAddress) / PAGE_SIZE;

        if (PagingGetTableMapping(Address) != 0 ||
            MappingIndex >= Section->NumberOfPages)
        {
            KeClearCustomThreadAddressSpace(KeGetCurrentThread());
            ObDereferenceObject(Section);
            return STATUS_CONFLICTING_ADDRESSES;
        }
    }

    /* Actually map the section pages into the address space. */
    for (Index = 0; Index < SectionView->SizePages; Index++)
    {
        Address = VirtualAddress + Index * PAGE_SIZE;
        MappingIndex = (Address - SectionView->BaseAddress) / PAGE_SIZE + SectionView->MappingStart;
 
        Status = PagingMapPage(
            Address, 
            PA_FROM_PFN(Section->Mappings[MappingIndex]), 
            PagingFlagsFromSectionFlags(
                Section->Mappings[MappingIndex],
                Section,
                SectionView));
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
            ObDereferenceObject(Section);
            return Status;
        }
    }

    KeClearCustomThreadAddressSpace(KeGetCurrentThread());
    ObDereferenceObject(Section);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
MmCreateView(
    PADDRESS_SPACE AddressSpace,
    HANDLE hSection,
    ULONG_PTR VirtualAddress,
    ULONG_PTR MappingStart,
    ULONG_PTR FileOffset,
    ULONG Protection,
    SIZE_T Size,
    PSECTION_VIEW* pOutView)
{
    KIRQL Irql;
    PSECTION_VIEW View;
    NTSTATUS Status;
    PKMEMORY_SECTION Section;

    Size = PAGE_ALIGN(Size + PAGE_SIZE - 1);
    VirtualAddress = PAGE_ALIGN(VirtualAddress);

    /* Do not dereference on success - the view holds a reference to the section
     * object. */
    Status = ObReferenceObjectByHandle(
        hSection,
        0,
        NULL,
        KernelMode,
        &Section,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = KeSetCustomThreadAddressSpace(KeGetCurrentThread(), AddressSpace);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(Section);
        return Status;
    }

    KeAcquireSpinLock(&AddressSpace->Lock, &Irql);
    KeAcquireSpinLockAtDpcLevel(&Section->Lock);

    if (Size == 0)
    {
        Size = Section->NumberOfPages * PAGE_SIZE;
    }

    /* FIXME: This is a quick and dirty hack to get Ldr working. When a view is
       created at an address greater than the max address for a given section,
       the section gets expanded. Instead, Ldr should just create a section big
       enough to hold every address, and change the section associated with the
       fiile object. */
    if (MappingStart + Size / PAGE_SIZE > Section->NumberOfPages)
    {
        Status = MiResizeSection(Section, MappingStart + Size / PAGE_SIZE);
        if (!NT_SUCCESS(Status))
        {
            KeReleaseSpinLockFromDpcLevel(&Section->Lock);
            ObDereferenceObject(Section);
            goto Exit;
        }
    }

    if (VirtualAddress == NULL)
    {
        VirtualAddress = PagingFindFreePages(
            PAGING_USER_SPACE,
            PAGING_USER_SPACE_END,
            Size / PAGE_SIZE + 1);
    }

    View = ExAllocatePoolWithTag(NonPagedPool, sizeof(*View), 'MSEL');
    if (View == NULL)
    {
        Status = STATUS_NO_MEMORY;
        KeReleaseSpinLockFromDpcLevel(&Section->Lock);
        ObDereferenceObject(Section);
        goto Exit;
    }
    View->hSection = hSection;
    View->Name = NULL;
    View->BaseAddress = VirtualAddress;
    View->SizePages = Size / PAGE_SIZE;
    View->FileOffset = FileOffset;
    View->MappingStart = MappingStart;
    View->Protection = Protection;

    Status = MiMapView(AddressSpace, View);
    if (!NT_SUCCESS(Status))
    {
        KeReleaseSpinLockFromDpcLevel(&Section->Lock);
        ObDereferenceObject(Section);
        goto Exit;
    }

    InsertTailList(&AddressSpace->SectionViewHead, &View->AddressSpaceEntry);
    *pOutView = View;
    KeReleaseSpinLockFromDpcLevel(&Section->Lock);
Exit:
    KeReleaseSpinLock(&AddressSpace->Lock, Irql);
    Status = KeClearCustomThreadAddressSpace(KeGetCurrentThread());
    MiNotifyAboutSectionChanges();
    return Status;
}

NTSTATUS
NTAPI
MmDeleteView(
    PADDRESS_SPACE AddressSpace,
    PSECTION_VIEW View)
{
    KIRQL Irql;
    SIZE_T Index;
    NTSTATUS Status;
    PKMEMORY_SECTION Section;

    Status = ObReferenceObjectByHandle(
        View->hSection,
        0,
        NULL,
        KernelMode,
        &Section,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = KeSetCustomThreadAddressSpace(KeGetCurrentThread(), AddressSpace);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(View->hSection);
        return Status;
    }
    
    KeAcquireSpinLock(&AddressSpace->Lock, &Irql);
    KeAcquireSpinLockAtDpcLevel(&Section->Lock);

    for (Index = 0; Index < View->SizePages; Index++)
    {
        PagingMapPage(View->BaseAddress + Index * PAGE_SIZE, NULL, 0);
    }

    RemoveEntryList(&View->AddressSpaceEntry);
    ExFreePool(View);
    /* Remove view's reference to the section if successful. */
    ObDereferenceObject(Section);
    /* Remove this function's reference. */
    ObDereferenceObject(Section);
    MiNotifyAboutSectionChanges();
    KeReleaseSpinLockFromDpcLevel(&Section->Lock);
    KeReleaseSpinLock(&AddressSpace->Lock, Irql);

    if (View->Name != NULL)
    {
        ExFreePool(View->Name);
    }

    Status = KeClearCustomThreadAddressSpace(KeGetCurrentThread());
    return Status;
}

NTSTATUS
NTAPI
MmInitObjects()
{
    NTSTATUS Status;

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
    SIZE_T NumberOfPages,
    ULONG_PTR Flags,
    HANDLE File,
    PKMEMORY_SECTION *outSection)
{
    NTSTATUS Status;
    MEMORY_SECTION_CREATION_DATA Data;
    OBJECT_ATTRIBUTES Attributes;

    InitializeObjectAttributes(&Attributes, NULL, 0, NULL, NULL);

    Data.NumberOfPages = NumberOfPages;
    Data.Flags = Flags;
    Data.File = File;
    Status = ObCloneHandle(File, &Data.File);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObCreateObject(
        (PVOID*)outSection,
        0, 
        KernelMode, 
        &Attributes,
        MmSectionType, 
        &Data);

    return Status;
}

PADDRESS_SPACE
NTAPI
MmGetCurrentAddressSpace(VOID)
{
    ASSERT(KeGetCurrentThread()->ThreadLock);
    if (KeGetCurrentThread()->CustomAddressSpace != NULL)
    {
        return KeGetCurrentThread()->CustomAddressSpace;
    }

    return &KeGetCurrentThread()->Process->AddressSpace;
}

NTSTATUS
NTAPI
MmHandleViewPageFault(
    PSECTION_VIEW SectionView,
    ULONG_PTR Address,
    BOOLEAN Write)
{
    PKMEMORY_SECTION Section;
    NTSTATUS Status;
    KIRQL Irql;

    PrintT(
        "Page fault in view %s %X(%i):%X\n", 
        (SectionView->Name ? (SectionView->Name) : ("<??>")), 
        SectionView->BaseAddress, 
        SectionView->SizePages,
        Address);

    Status = ObReferenceObjectByHandle(
        SectionView->hSection,
        0,
        NULL,
        KernelMode,
        &Section,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        PrintT("Bad section\n");
        return Status;
    }

    KeAcquireSpinLock(&Section->Lock, &Irql);
    if (Section->SectionFile != NULL && !Write)
    {
        LARGE_INTEGER Offset;
        IO_STATUS_BLOCK StatusBlock;
        SIZE_T MappingIndex;
        ULONG_PTR FileOffset;
        PFN_NUMBER Pfn;

        FileOffset = PAGE_ALIGN(Address - SectionView->BaseAddress) + SectionView->FileOffset;

        MappingIndex = (Address - SectionView->BaseAddress) / PAGE_SIZE + SectionView->MappingStart;
        Offset.QuadPart = FileOffset;

        if (Section->Mappings[MappingIndex] == NULL)
        {
            Status = MmAllocatePfn(&Section->Mappings[MappingIndex]);
            if (!NT_SUCCESS(Status))
            {
                KeReleaseSpinLock(&Section->Lock, Irql);
                ObDereferenceObject(Section);
                return Status;
            }

            Pfn = Section->Mappings[MappingIndex];

            PagingMapPage(
                PAGE_ALIGN(Address),
                PA_FROM_PFN(Pfn),
                PAGE_PRESENT | PAGE_WRITE);
        }

        ASSERTMSG(KeGetCurrentIrql() <= DISPATCH_LEVEL, "");

        Status = NtReadFile(Section->SectionFile,
                          NULL, 
                          NULL, 
                          NULL, 
                          &StatusBlock, 
                          (PVOID)PAGE_ALIGN(Address),
                          PAGE_SIZE,
                          &Offset,
                          NULL);

        KeReleaseSpinLock(&Section->Lock, Irql);
        ObDereferenceObject(Section);
        return Status;
    }
    else if (Section->SectionFile != NULL && Write)
    {
        /* TODO */
        ASSERT(FALSE);
        KeReleaseSpinLock(&Section->Lock, Irql);
        return STATUS_NOT_SUPPORTED;
    }

    KeReleaseSpinLock(&Section->Lock, Irql);
    ObDereferenceObject(Section);
    return STATUS_INVALID_ADDRESS;
}

static
BOOLEAN
MmIsAddressInView(
    PSECTION_VIEW View,
    ULONG_PTR Address)
{
    ULONG_PTR Base = View->BaseAddress;
    SIZE_T Size = View->SizePages * PAGE_SIZE;

    return (Address >= Base) && (Address < Base + Size);
}

NTSTATUS
NTAPI
MmHandlePageFault(
    ULONG_PTR Address,
    BOOLEAN Write)
{
    PADDRESS_SPACE pCurrentAddressSpace;
    PLIST_ENTRY Current, Head;
    KIRQL Irql;
    NTSTATUS Status;

    KeAcquireSpinLock(&KeGetCurrentThread()->ThreadLock, &Irql);
    pCurrentAddressSpace = MmGetCurrentAddressSpace();

    /* Lock the address space. */
    KeAcquireSpinLockAtDpcLevel(&pCurrentAddressSpace->Lock);
    Current = pCurrentAddressSpace->SectionViewHead.First;
    Head = &pCurrentAddressSpace->SectionViewHead;

    /* Iterate over each view in the address space. */
    while (Current != Head)
    {
        PSECTION_VIEW CurrentView = 
            CONTAINING_RECORD(Current, 
                              SECTION_VIEW, 
                              AddressSpaceEntry);

        /* If the address falls into this section, try to handle it. */
        if (MmIsAddressInView(CurrentView, Address))
        {
            Status = MmHandleViewPageFault(CurrentView, Address, Write);
            MiNotifyAboutSectionChanges();
            KeReleaseSpinLockFromDpcLevel(&pCurrentAddressSpace->Lock);
            KeReleaseSpinLock(&KeGetCurrentThread()->ThreadLock, Irql);
            /* Address was handled, exit. */
            return Status;
        }

        /* Address was not handled, go to the next section. */
        Current = Current->Next;
    }

    /* Address could not be handled, this is an access violation. */
    MiNotifyAboutSectionChanges();
    KeReleaseSpinLockFromDpcLevel(&pCurrentAddressSpace->Lock);
    KeReleaseSpinLock(&KeGetCurrentThread()->ThreadLock, Irql);
    return STATUS_INVALID_PARAMETER;
}

NTSYSAPI
NTSTATUS
NTAPI
NtReferenceSectionFromFile(
    PHANDLE pOutHandle,
    PUNICODE_STRING pFilepath)
{
    HANDLE hFile;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatus;
    PKMEMORY_SECTION MemorySection;
    LARGE_INTEGER FileSize;
    HANDLE hSection;

    InitializeObjectAttributes(&objAttr, pFilepath, 0, NULL, NULL);
    Status = NtCreateFile(
        &hFile, 
        FILE_GENERIC_READ, 
        &objAttr, 
        &ioStatus, 
        NULL, 
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        NULL, 
        0);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    hSection = NULL;
    Status = NnxGetImageSection(hFile, &hSection);
    /* If the file has an associated image section, or there was an error, 
     * return the status. */
    if (!NT_SUCCESS(Status) || hSection != NULL)
    {
        if (hSection != NULL)
        {
            ObReferenceObjectByHandle(hSection, 0, NULL, KernelMode, NULL, NULL);
            *pOutHandle = hSection;
        }
        ObCloseHandle(hFile, KernelMode);
        return Status;
    }

    /* The initial size of a file backed memory section is equal to the file size
     * page aligned up. */
    Status = NnxGetNtFileSize(hFile, &FileSize);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(hFile, KernelMode);
        return Status;
    }

    Status = MiCreateSectionObject(
        (FileSize.QuadPart + PAGE_SIZE - 1) / PAGE_SIZE, 
        0, 
        hFile, 
        &MemorySection);
    if (!NT_SUCCESS(Status))
    {
        ObCloseHandle(hFile, KernelMode);
        return Status;
    }

    Status = ObCreateHandle(&hSection, KernelMode, MemorySection);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(MemorySection);
        ObCloseHandle(hFile, KernelMode);
        return Status;
    }
    
    /* Additional reference held by the handle. */
    ObDereferenceObject(MemorySection);

    Status = NnxSetImageSection(hFile, hSection);
    if (!NT_SUCCESS(Status))
    {
        ObDereferenceObject(MemorySection);
        ObCloseHandle(hFile, KernelMode);
        ObCloseHandle(hSection, KernelMode);
        return Status;
    }
    *pOutHandle = hSection;

    ObCloseHandle(hFile, KernelMode);
    return Status;
}