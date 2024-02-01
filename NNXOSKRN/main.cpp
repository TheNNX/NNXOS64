#include <HALX64/include/ACPI.h>
#include <HALX64/include/MP.h>
#include <HALX64/include/APIC.h>
#include <HALX64/include/PIT.h>
#include <HALX64/include/cmos.h>

#include <SimpleTextIo.h>
#include <nnxcfg.h>
#include <paging.h>
#include <mm.h>
#include <vfs.h>
#include <bugcheck.h>
#include <cpu.h>
#include <pcr.h>
#include <scheduler.h>
#include "nnxver.h"
#include <object.h>
#include <pool.h>
#include <Keyboard.h>
#include <rtc.h>
#include <syscall.h>
#include <file.h>
#include <ldr.h>

extern "C"
{
    VOID UpdateScreen();
    VOID DrawMap();
    volatile UINT32* gFramebuffer;
    volatile UINT32* gFramebufferEnd;
    UINT32 gPixelsPerScanline;
    UINT32 gWidth;
    UINT32 gHeight;
    extern UINT32 gMinY;
    extern UINT32 gMinX;
    extern UINT32 gMaxY;
    extern UINT32 gMaxX;
    extern UINT8 Initialized;
    extern UINT64 MemorySize;

    ULONG_PTR gRdspPhysical;
    ULONG_PTR KeMaximumIncrement = 15 * 10000;
    UINT KeNumberOfProcessors = 1;

    VOID 
    KeExceptionHandler(
        UINT64 n, 
        UINT64 errcode, 
        UINT64 errcode2, 
        UINT64 rip)
    {
        KeBugCheckEx(
            KMODE_EXCEPTION_NOT_HANDLED, 
            n,
            rip, 
            errcode, 
            errcode2);
    }

    __declspec(dllimport) const char* HalTest();

    static
    NTSTATUS
    TestFileIo()
    {
        HANDLE hFile;
        NTSTATUS Status;
        IO_STATUS_BLOCK StatusBlock;
        OBJECT_ATTRIBUTES FileAttributes;
        LARGE_INTEGER ByteOffset;
        CHAR Buffer[513] = { 0 };
        UNICODE_STRING TestFileName = RTL_CONSTANT_STRING(L"EFI\\BOOT\\FILE.ASM");

        InitializeObjectAttributes(
            &FileAttributes,
            &TestFileName,
            0,
            NULL,
            NULL);

        PrintT("Create file\n");
        Status = NtCreateFile(&hFile, FILE_READ_DATA,
            &FileAttributes,
            &StatusBlock,
            0,
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

        ByteOffset.QuadPart = 69;

        PrintT("Read file\n");
        Status = NtReadFile(
            hFile,
            NULL,
            NULL,
            NULL,
            &StatusBlock,
            Buffer,
            512,
            &ByteOffset,
            NULL);
        if (!NT_SUCCESS(Status))
        {
            return Status;
        }

        for (int i = 0; i < 512; i++)
        {
            PrintT("%c", Buffer[i]);
        }

        return STATUS_SUCCESS;
    }

    __declspec(dllexport) 
    UINT64 
    KeEntry()
    {
        NTSTATUS Status;
        ULONG64 CurrentTime;

        HalpInitDummyPcr();
        HalpSetDummyPcr();

        HalDisableInterrupts();
        PitUniprocessorInitialize();

        gExceptionHandlerPtr = KeExceptionHandler;

        TextIoInitialize(
            gFramebuffer, 
            gFramebufferEnd, 
            gWidth,
            gHeight,
            gPixelsPerScanline);

        TextIoClear();
        MmReinitPhysAllocator(PfnEntries, NumberOfPfnEntries);

        Status = ExInitializePool(
            (PVOID)PagingAllocatePageBlockFromRange(
                32, 
                PAGING_KERNEL_SPACE,
                PAGING_KERNEL_SPACE_END),
            PAGE_SIZE * 32,
            (PVOID)PagingAllocatePageBlockFromRange(
                64, 
                PAGING_KERNEL_SPACE,
                PAGING_KERNEL_SPACE_END),
            PAGE_SIZE * 64);
        if (!NT_SUCCESS(Status))
        {
            KeBugCheck(HAL_INITIALIZATION_FAILED);
        }

        Status = ExPoolSelfCheck();
        if (!NT_SUCCESS(Status))
        {
            KeBugCheck(HAL_INITIALIZATION_FAILED);
        }

        DrawMap();
        VfsInit();

        PciScan();

        ACPI_RDSP* pRdsp = (ACPI_RDSP*) 
            PagingMapStrcutureToVirtual(
                gRdspPhysical, 
                sizeof(ACPI_RDSP), 
                PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);

        UINT8 bAcpiError;
        ACPI_FADT* pFacp = (ACPI_FADT*) AcpiGetTable(pRdsp, "FACP");

        if (!pFacp)
        {
            bAcpiError = AcpiLastError();
            AcpiError(bAcpiError);

            KeBugCheckEx(
                PHASE1_INITIALIZATION_FAILED, 
                __LINE__, 
                bAcpiError, 
                0,
                0);
        }

        ACPI_MADT* pMadt = (ACPI_MADT*) AcpiGetTable(pRdsp, "APIC");

        if (!NT_SUCCESS(KeInitializeDispatcher()))
        {
            KeBugCheckEx(
                PHASE1_INITIALIZATION_FAILED, 
                __LINE__, 
                0, 
                0, 
                0);
        }

        ApicInit(pMadt);

        SetupSystemCallHandler(SystemCallHandler);
        HalpSetupPcrForCurrentCpu(ApicGetCurrentLapicId());
        KeyboardInitialize();

        PrintT(
            "%s %i.%i.%i.%i, compiled %s %s\n",
            NNX_OSNAME, 
            NNX_MAJOR, 
            NNX_MINOR, 
            NNX_PATCH, 
            NNX_BUILD,
            __DATE__,
            __TIME__);

        Status = ObInit();
        if (!NT_SUCCESS(Status))
        {
            KeBugCheckEx(
                PHASE1_INITIALIZATION_FAILED, 
                __LINE__, 
                Status, 
                0,
                0);
        }

        PagingInitializePageFile(
            16 * PAGE_SIZE,
            "PAGEFILE.SYS", 
            VfsGetSystemVfs());

        MmInitObjects();
        CmosInitialize();
        HalRtcInitialize(pFacp->CenturyRegister);
        
        KeQuerySystemTime(&CurrentTime);
        PrintT("Current date and time (%i): ", CurrentTime);
        HalpPrintCurrentDate();
        PrintT(" ");
        HalpPrintCurrentTime();
        PrintT("\n");

        NtFileObjInit();

        MpInitialize();

        KeBugCheck(PHASE1_INITIALIZATION_FAILED);
    }
}