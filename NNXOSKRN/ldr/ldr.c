#include <scheduler_internal.h>
#include <mm.h>
#include <pool.h>
#include <SimpleTextIO.h>
#include <rtl.h>
#include <nnxpe.h>
#include <ntdebug.h>

typedef struct _LDR_MODULE
{
    LIST_ENTRY      ProcessEntry;
    HANDLE          hSection;
    ULONG_PTR       PreferredBaseAddress;
    ULONG_PTR       BaseAddress;
    PSECTION_VIEW*  SectionViews;
    ULONG_PTR       EntrypointRVA;
} LDR_MODULE, * PLDR_MODULE;

static UNICODE_STRING NtdllPath = RTL_CONSTANT_STRING(L"NTDLL.DLL");
static const ULONG DummyTag = 'THRD';
static const ULONG LdrmTag = 'LDRM';

NTSTATUS
NTAPI
NnxStartUserProcess(
    PCUNICODE_STRING Filepath,
    HANDLE hOutProcess,
    ULONG Priority)
{
    PEPROCESS pProcess;
    PETHREAD pThread;
    NTSTATUS status;
    PUNICODE_STRING strCopy;

    /* Create a copy of the filepath for the loader thread. This copy is owned
     * by that thread - it is responsible for freeing it. This is done, so the
     * caller can modify/free the memory pointed to by Firepath after returning
     * from this function. */
    strCopy = ExAllocatePoolWithTag(NonPagedPool, sizeof(UNICODE_STRING), DummyTag);
    if (strCopy == NULL)
    {
        return STATUS_NO_MEMORY;
    }

    strCopy->Buffer = ExAllocatePoolWithTag(
        NonPagedPool,
        Filepath->MaxLength * sizeof(*strCopy->Buffer),
        DummyTag);
    if (strCopy->Buffer == NULL)
    {
        ExFreePoolWithTag(strCopy, DummyTag);
        return STATUS_NO_MEMORY;
    }

    strCopy->Length = Filepath->Length;;
    strCopy->MaxLength = Filepath->MaxLength;
    RtlCopyMemory(strCopy->Buffer, Filepath->Buffer, strCopy->Length);

    /* Create the process and its initialzer thread. */
    status = PspCreateProcessInternal(&pProcess, 0, NULL);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    pProcess->Pcb.BasePriority = Priority;

    status = PspCreateThreadInternal(
        &pThread,
        pProcess,
        TRUE,
        (ULONG_PTR)NnxDummyLdrThread);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    PspSetUsercallParameter(&pThread->Tcb, 0, (ULONG_PTR)strCopy);
    pThread->Tcb.ThreadPriority = 0;

    PspInsertIntoSharedQueueLocked(&pThread->Tcb);
    return ObCreateHandle(hOutProcess, 0, pProcess);
}

NTSTATUS
NTAPI
NnxLdrCreateSectionSubviews(
    HANDLE hSection,
    PSECTION_VIEW pInitialView,
    PLDR_MODULE pModule)
{
    NTSTATUS Status;
    PIMAGE_DOS_HEADER dosHeader;
    PIMAGE_PE_HEADER peHeader;
    PIMAGE_SECTION_HEADER sectionHeaders, sectionHeadersInFile;
    UINT16 numberOfSections;
    ULONG_PTR baseAddress;
    ULONG i, j;
    PSECTION_VIEW view;
    PADDRESS_SPACE pAddressSpace;
    ULONG protection;
    ULONG characteristics;

    dosHeader = (PIMAGE_DOS_HEADER) pInitialView->BaseAddress;
    if (dosHeader->Signature != IMAGE_MZ_MAGIC)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    peHeader = (PIMAGE_PE_HEADER)
        (pInitialView->BaseAddress + dosHeader->e_lfanew);
    if (peHeader->Signature != IMAGE_PE_MAGIC)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    numberOfSections = peHeader->FileHeader.NumberOfSections;

    pModule->SectionViews = 
        ExAllocatePoolWithTag(
            NonPagedPool, 
            numberOfSections * sizeof(PSECTION_VIEW), 
            LdrmTag);
    pModule->EntrypointRVA = peHeader->OptionalHeader.EntrypointRVA;
    
    sectionHeadersInFile = (PIMAGE_SECTION_HEADER)
        ((ULONG_PTR)peHeader + sizeof(*peHeader) +
        peHeader->OptionalHeader.NumberOfDataDirectories * 
            sizeof(IMAGE_DATA_DIRECTORY));

    /* Create a copy of the section headers. */
    sectionHeaders = 
        ExAllocatePool(
            NonPagedPool, 
            numberOfSections * sizeof(IMAGE_SECTION_HEADER));
    RtlCopyMemory(
        sectionHeaders, 
        sectionHeadersInFile, 
        numberOfSections * sizeof(IMAGE_SECTION_HEADER));

    baseAddress = peHeader->OptionalHeader.ImageBase;
    PrintT("Image base %X, entrypoint: %X\n", baseAddress, pModule->EntrypointRVA);
    pModule->BaseAddress = baseAddress;
    pModule->PreferredBaseAddress = baseAddress;

    pAddressSpace = &KeGetCurrentProcess()->Pcb.AddressSpace;

    /* Delete the initial view - until subviews are created, the contents of the 
       file are inaccesible. This is why a copy of section headers was made. */
    Status = 
        MmDeleteView(
            pAddressSpace,
            pInitialView);
    if (!NT_SUCCESS(Status))
    {
        ExFreePool(sectionHeaders);
        ExFreePool(pModule->SectionViews);
        pModule->SectionViews = NULL;
        return Status;
    }

    for (i = 0; i < numberOfSections; i++)
    {
        /* FIXME: relocation support. */
        ASSERT(sectionHeaders[i].NumberOfRelocations == 0);

        protection = PAGE_READONLY;
        characteristics = sectionHeaders[i].Characteristics;

        if (characteristics & IMAGE_SCN_MEM_WRITE)
        {
            protection = PAGE_READWRITE;
        }

        Status =
            MmCreateView(
                pAddressSpace, 
                hSection, 
                baseAddress + sectionHeaders[i].VirtualAddressRVA, 
                sectionHeaders[i].VirtualAddressRVA / PAGE_SIZE,
                sectionHeaders[i].PointerToDataRVA,
                protection,
                sectionHeaders[i].VirtualSize, 
                &view);
        if (!NT_SUCCESS(Status))
        {
            PrintT("View creation failed: NTSTATUS=0x%X\n", Status);
            ExFreePool(sectionHeaders);
            ExFreePool(pModule->SectionViews);
            pModule->SectionViews = NULL;
            return Status;
        }

        view->Name = ExAllocatePool(NonPagedPool, 9);
        for (j = 0; j < 8; j++)
        {
            view->Name[j] = sectionHeaders[i].Name[j];
        }
        view->Name[j] = 0;
    }

    ExFreePool(sectionHeaders);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
NnxLdrCreateModule(
    HANDLE hSection,
    PSECTION_VIEW pInitialView,
    PLDR_MODULE* ppOutModule)
{
    PLDR_MODULE pModule;
    PKPROCESS pProcess;
    NTSTATUS Status;
    KIRQL Irql;

    pModule = ExAllocatePoolWithTag(NonPagedPool, sizeof(*pModule), LdrmTag);
    pModule->SectionViews = NULL;
    pModule->EntrypointRVA = NULL;
    pModule->hSection = hSection;
    pModule->PreferredBaseAddress = NULL;
    pModule->BaseAddress = NULL;

    Status = NnxLdrCreateSectionSubviews(hSection, pInitialView, pModule);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(pModule, LdrmTag);
        return Status;
    }

    pProcess = &KeGetCurrentProcess()->Pcb;

    KeAcquireSpinLock(&pProcess->ProcessLock, &Irql);
    InsertTailList(&pProcess->LdrModulesHead, &pModule->ProcessEntry);
    KeReleaseSpinLock(&pProcess->ProcessLock, Irql);

    *ppOutModule = pModule;

    return STATUS_SUCCESS;
}

VOID
NTAPI
NnxDummyLdrThread(
    PUNICODE_STRING Filepath)
{
    NTSTATUS Status;
    HANDLE Section;
    PSECTION_VIEW View;
    PLDR_MODULE pModule;
    PBYTE entrypoint;
    PETHREAD mainThread;

    Status = STATUS_SUCCESS;

    /* TODO: Test if opening the section for the second time works as intented. */
    Status = NtReferenceSectionFromFile(&Section, Filepath);
    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(Filepath->Buffer, DummyTag);
        ExFreePoolWithTag(Filepath, DummyTag);
        PsExitThread(Status);
    }

    Status = MmCreateView(
        &KeGetCurrentProcess()->Pcb.AddressSpace,
        Section,
        0x4321000 /* FIXME */,
        0,
        0,
        PAGE_READONLY,
        0,
        &View);

    if (!NT_SUCCESS(Status))
    {
        ExFreePoolWithTag(Filepath->Buffer, DummyTag);
        ExFreePoolWithTag(Filepath, DummyTag);
        PsExitThread(Status);
    }

    ExFreePoolWithTag(Filepath->Buffer, DummyTag);
    ExFreePoolWithTag(Filepath, DummyTag);

    Status = NnxLdrCreateModule(Section, View, &pModule);
    if (!NT_SUCCESS(Status))
    {
        PsExitThread(Status);
    }

    if (pModule->EntrypointRVA != NULL)
    {
        entrypoint = (PBYTE)(pModule->EntrypointRVA + pModule->BaseAddress);
 
        /* The current loader thread could be reused to host the main process 
           thread. However, this isn't simple, as the thread has to jump to 
           usermode. While this could be done, with how the scheduler interrupt
           is implemented currently, this requires some stack manipultion. */

        /* Create a new thread for now. */
        Status = PspCreateThreadInternal(
            &mainThread, 
            KeGetCurrentProcess(), 
            FALSE, 
            (ULONG_PTR)entrypoint);
        if (!NT_SUCCESS(Status))
        {
            PrintT("Ldr thread error: %X\n", Status);
            PsExitThread((DWORD)Status);
        }

        PspInsertIntoSharedQueueLocked(&mainThread->Tcb);
    }

    /* This message cannot be printed in a parent thread that waits for this
       threads termination, because this thread can be started before there are
       any threads, that could wait for this thread. */
    PrintT("Exiting Ldr thread with status: %X\n", Status);
    PsExitThread(STATUS_SUCCESS);
}