#define NNX_ALLOC_DEBUG 1
#include <nnxalloc.h>
#include <HALX64/include/MP.h>
#include <HALX64/include/APIC.h>
#include <HALX64/include/PIT.h>
#include <paging.h>
#include <vfs.h>
#include <pcr.h>
#include <bugcheck.h>
#include <scheduler.h>

extern "C" {

    extern UINT KeNumberOfProcessors;
    PVOID* ApStackPointerArray;

    PVOID MpPopulateApStartupCode()
    {
        VFS_FILE* apCodeFile;
        VFS* systemPartition;
        PVOID code = (PVOID) 0x0000;
        PagingMapPage(0x0000, 0x0000, PAGE_WRITE | PAGE_PRESENT);
        _AP_DATA* data = (_AP_DATA*) (((UINT64) code) + 0x800);
        systemPartition = VfsGetSystemVfs();

        apCodeFile = systemPartition->Functions.OpenFile(
            systemPartition, 
            (char*)"APSTART.BIN");

        if (!apCodeFile)
        {
            PrintT("Error loading file\n");
            while (1);
        }
        else
        {
            systemPartition->Functions.ReadFile(apCodeFile, PAGE_SIZE, code);
            systemPartition->Functions.CloseFile(apCodeFile);
        }

        data->ApCR3 = __readcr3();
        data->ApStackPointerArray = ApStackPointerArray;
        data->ApProcessorInit = ApProcessorInit;
        data->ApLapicIds = ApicLocalApicIDs;
        data->ApNumberOfProcessors = ApicNumberOfCoresDetected;

        _sgdt(&data->ApGdtr);
        __sidt(&data->ApIdtr);

        return code;
    }

    UINT32 debugColors[] = {
        0xFF000000,
        0xFF0000AA,
        0xFF00AA00,
        0xFF00AAAA,
        0xFFAA0000,
        0xFFAA00AA,
        0xFFAA5500,
        0xFFAAAAAA,
        0xFF555555,
        0xFF5555FF,
        0xFF55FF55,
        0xFF55FFFF,
        0xFFFF5555,
        0xFFFF55FF,
        0xFFFFFF55,
        0xFFFFFFFF
    };

    UINT32 color = 0;

    VOID MpInitialize()
    {
        UINT64 i, j;
        UINT8 currentLapicId;
        PVOID apData, apCode;
        NTSTATUS status;

        currentLapicId = ApicGetCurrentLapicId();

        KeNumberOfProcessors = ApicNumberOfCoresDetected;
        PrintT("Number of detected processors %i, BSP LAPIC ID %i\n", KeNumberOfProcessors, currentLapicId);

        if (ApicNumberOfCoresDetected != 1)
        {
            ApStackPointerArray = (PVOID*)ExAllocatePool(
                NonPagedPool,
                ApicNumberOfCoresDetected * sizeof(*ApStackPointerArray));

            apCode = MpPopulateApStartupCode();
            apData = (VOID*)(((ULONG_PTR)apCode) + 0x800);

            ApicClearError();
            for (i = 0; i < ApicNumberOfCoresDetected; i++)
            {
                if (ApicLocalApicIDs[i] == currentLapicId)
                {
                    continue;
                }

                ApStackPointerArray[i] =
                    (PVOID)(PagingAllocatePageFromRange(
                        PAGING_KERNEL_SPACE,
                        PAGING_KERNEL_SPACE_END) + PAGE_SIZE);

                ApicInitIpi(ApicLocalApicIDs[i], 0x00);
                PitUniprocessorPollSleepMs(10);

                for (j = 0; j < 2; j++)
                {
                    ApicClearError();

                    if ((UINT64)apCode > UINT16_MAX)
                    {
                        KeBugCheck(HAL_INITIALIZATION_FAILED);
                    }

                    ApicStartupIpi(ApicLocalApicIDs[i], 0, (UINT16)(ULONG_PTR)apCode);
                    PitUniprocessorPollSleepUs(200);
                }

#if 0
                while (1)
                {
                    PrintT("core %i->%i result: %X\n", i, ApicLocalApicIDs[i], ((_AP_DATA*)apData)->Output);
                    PitUniprocessorPollSleepMs(750);
                }
#endif
            }
        }

        /* BSP is always core 0. */
        status = PspInitializeCore(0);
        PrintT("PspInitializeCore NTSTATUS: %X\n", status);
        if (!NT_SUCCESS(status))
        {
            KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);
        }
    }

    VOID ApProcessorInit(UINT8 coreNumber)
    {
        NTSTATUS status;

        HalpSetDummyPcr();

        HalpSetupPcrForCurrentCpu(coreNumber);
        ApicLocalApicInitializeCore();
        status = PspInitializeCore(coreNumber);

        if (!NT_SUCCESS(status))
        {
            KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, status, 0, 0, 0);
        }

        /* The system shouldn't get here anyway. */
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED, 0, 0, 0, 0);
    }
}