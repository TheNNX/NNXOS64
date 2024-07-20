#include <scheduler.h>
#include <object.h>
#include <bugcheck.h>
#include <mm.h>

UNICODE_STRING CsrssName = RTL_CONSTANT_STRING(L"CSRSS.EXE");
UNICODE_STRING WinlogonName = RTL_CONSTANT_STRING(L"WINLOGON.EXE");

NTSTATUS Main()
{
    HANDLE hCsrss, hWinlogon;
    HANDLE hCsrssSection;
    HANDLE hWinlogonSection;
    NTSTATUS WaitStatus, Status;
    HANDLE WaitTable[2];
    OBJECT_ATTRIBUTES ObjectAttributes;

    while (1);

    /* Start Csrss */
    Status = NtReferenceSectionFromFile(&hCsrssSection, &CsrssName);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    InitializeObjectAttributes(&ObjectAttributes, &CsrssName, 0, NULL, NULL);
    Status = NtCreateProcess(
        &hCsrss, 
        0, 
        &ObjectAttributes, 
        NULL, 
        FALSE, 
        hCsrssSection, 
        NULL, 
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    /* Start Winlogon */
    Status = NtReferenceSectionFromFile(&hWinlogonSection, &WinlogonName);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    InitializeObjectAttributes(&ObjectAttributes, &WinlogonName, 0, NULL, NULL);
    Status = NtCreateProcess(
        &hWinlogon,
        0,
        &ObjectAttributes,
        NULL,
        FALSE,
        hWinlogonSection,
        NULL,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WaitTable[0] = hCsrss;
    WaitTable[1] = hWinlogon;
    WaitStatus = KeWaitForMultipleObjects(
        2,
        WaitTable,
        WaitAny, 
        Executive,
        KernelMode,
        FALSE,
        NULL, 
        NULL);

    KeBugCheckEx(CRITICAL_PROCESS_DIED, WaitTable[WaitStatus], 0, 0, 0);
}