#define NNX_ALLOC_DEBUG 1
#include <nnxalloc.h>
#include "APIC.h"
#include "PIC.h"
#include <HAL/paging.h>
#include <HAL/physical_allocator.h>
#include <nnxlog.h>
#include <device/fs/fat.h>
#include "PIT.h"
#include "../spinlock.h"
#include <HAL/cpu.h>
#include <rtl/rtl.h>

extern "C" 
{
	ULONG_PTR ApicLocalApicBase;
	ULONG_PTR ApicVirtualLocalApicBase;
	UINT ApicNumberOfCoresDetected;
	UINT8* ApicLocalApicIDs;
	UINT ApicNumberOfCoresInitialized = 0;

	BOOL ApicBspLapicInitialized = FALSE;
	ULONG_PTR HalpTimerTicksPerSecond = 0;
	extern UINT8* GlobalPhysicalMemoryMap;
	extern UINT64 GlobalPhysicalMemoryMapSize;
	extern UINT64 MemorySize;

	VOID KiEnableInterrupts();
}

IoApic** IoApics;
UINT64 IoApicCount;

extern "C"
{
	extern ULONG_PTR KeMaximumIncrement;
	ULONG_PTR KiCyclesPerClockQuantum;
	ULONG_PTR KiCyclesPerQuantum;

	BOOLEAN ApicInitIsaIrq(DWORD* IsaIrqGsiMapping, BYTE Irqn, BYTE TargetVector)
	{
		for (int i = 0; i < IoApicCount; i++)
		{
			IoApic* ioApic = IoApics[i];
			if (ioApic->InterruptRangeCheck(IsaIrqGsiMapping[Irqn]))
			{
				ioApic->WriteRedirectionEntry(IsaIrqGsiMapping[Irqn], ((UINT64)TargetVector) | 0x0100000000000900ULL);
				return TRUE;
			}
		}

		return FALSE;
	}

	VOID ApicInit(ACPI_MADT* madt)
	{
		UINT64 i = 0, currentLocalApicIdIndex = 0, currentIoApicIndex = 0;
		UINT8* currentPointer = (UINT8*)&(madt->InteruptControlerStruct);
		NNXLogger* logger = new NNXLogger(VfsGetSystemVfs()->Functions.OpenOrCreateFile(VfsGetSystemVfs(), (char*)"APICLOG.TXT"), false);
        
		DWORD irqSourceToGsiMapping[256];
		
		for (i = 0; i < 256; i++)
		{
			irqSourceToGsiMapping[i] = (DWORD)i;
		}

		logger->Log("APIC initialization started\n");

		ApicLocalApicBase = madt->LapicBase32;

		for (i = 0; i < madt->Header.Lenght - sizeof(ACPI_MADT); )
		{
			ACPI_MADT_ENTRY* madtEntry = ((ACPI_MADT_ENTRY*) (currentPointer + i));
			UINT8 type = madtEntry->Type;

			if (type == ACPI_MADT_TYPE_LAPIC)
			{
				ACPI_MADT_LAPIC* lapic = (ACPI_MADT_LAPIC*) madtEntry;
				ApicNumberOfCoresDetected += (lapic->Flags & 1) ? 1 : 0;
			}
			else if (type == ACPI_MADT_TYPE_IOAPIC)
			{
				ACPI_MADT_IOAPIC* ioapic = (ACPI_MADT_IOAPIC*) madtEntry;
				logger->Log("IOAPIC %i[%x] starting at global INT %x\n",
					(UINT64) ioapic->IoApicId, (UINT64) ioapic->IoApicAddress, (UINT64) ioapic->IoApicInterruptBase);
				IoApicCount++;
			}
			else if (type == ACPI_MADT_TYPE_SOURCE_OVERRIDE)
			{
				ACPI_MADT_SOURCE_OVERRIDE* sourceOverride = (ACPI_MADT_SOURCE_OVERRIDE*) madtEntry;
				logger->Log("Interrupt mapping: %x.%x -> %x, flags %x\n",
					(UINT64) sourceOverride->Bus, (UINT64) sourceOverride->Source, (UINT64) sourceOverride->GlobalSystemInterrupt, (UINT64) sourceOverride->Flags);
				
				irqSourceToGsiMapping[sourceOverride->Source] = sourceOverride->GlobalSystemInterrupt;
			}
			else if (type == ACPI_MADT_TYPE_IOAPIC_NMI_SOURCE)
			{
				ACPI_MADT_IOAPIC_NMI_SOURCE* ioApicNmi = (ACPI_MADT_IOAPIC_NMI_SOURCE*) madtEntry;
				logger->Log("Global interrupt %x is NMI\n", (UINT64) ioApicNmi->GlobalSystemInterrupt);
			}
			else if (type == ACPI_MADT_TYPE_LAPIC_NMI_SOURCE)
			{
				ACPI_MADT_LAPIC_NMI_SOURCE* lApicNmi = (ACPI_MADT_LAPIC_NMI_SOURCE*) madtEntry;
				logger->Log("Processor %x LINT%i is NMI\n", (UINT64) lApicNmi->ProcessorUid, (UINT64) lApicNmi->LapicLintN);
			}
			else if (type == ACPI_MADT_TYPE_LAPIC_ADDRESS_64)
			{
				ACPI_MADT_LAPIC_ADDRESS_64* lapicOverride = (ACPI_MADT_LAPIC_ADDRESS_64*) madtEntry;
				ApicLocalApicBase = lapicOverride->AddressOverride;
				logger->Log("LAPIC address override present: 0x%X\n", ApicLocalApicBase);
			}
			else if (type == ACPI_MADT_TYPE_X2LAPIC)
			{
				// TODO
			}

			i += madtEntry->Length;
		}

		ApicLocalApicIDs = (UINT8*) NNXAllocatorAllocArray(ApicNumberOfCoresDetected, sizeof(*ApicLocalApicIDs));
		IoApics = new IoApic*[IoApicCount];

		for (i = 0; i < madt->Header.Lenght - sizeof(ACPI_MADT); )
		{
			ACPI_MADT_ENTRY* madtEntry = ((ACPI_MADT_ENTRY*) (currentPointer + i));
			UINT8 type = madtEntry->Type;

			if (type == ACPI_MADT_TYPE_LAPIC)
			{
				ACPI_MADT_LAPIC* lapic = (ACPI_MADT_LAPIC*) madtEntry;
				logger->Log("LAPIC %i for UID %i%s\n", lapic->LapicID, lapic->ProcessorUid, (lapic->Flags & 1) ? (" [disabled]") : (""));
				if (lapic->Flags & 1)
				{
					ApicLocalApicIDs[currentLocalApicIdIndex++] = lapic->LapicID;
				}
			}
			else if (type == ACPI_MADT_TYPE_IOAPIC)
			{
#pragma warning(push)
				ACPI_MADT_IOAPIC* ioapic = (ACPI_MADT_IOAPIC*) madtEntry;
				logger->Log("Starting IOAPIC initialization\n");
#pragma warning(disable: 6386)
				IoApics[currentIoApicIndex] = new IoApic((volatile UINT32*)(ULONG_PTR)ioapic->IoApicAddress, ioapic->IoApicInterruptBase);
				currentIoApicIndex++;
#pragma warning(pop)
			}

			i += madtEntry->Length;
		}

		ApicInitIsaIrq(irqSourceToGsiMapping, 1, 33);

		PicDisableForApic();

		/* find virtual address space for the APIC registers */
		ApicVirtualLocalApicBase = (UINT64) PagingAllocatePageWithPhysicalAddress(
			PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END, 
			PAGE_PRESENT | PAGE_WRITE, PAGE_ALIGN(ApicLocalApicBase)
		);
		ApicVirtualLocalApicBase += (ApicLocalApicBase % PAGE_SIZE);

		/* ensure the physical page is marked as used in the memory map */
		GlobalPhysicalMemoryMap[ApicLocalApicBase / PAGE_SIZE] = MEM_TYPE_USED_PERM;

		ApicLocalApicInitializeCore();
	}

	VOID ApicLocalApicWriteRegister(UINT64 offset, UINT32 data)
	{
		UINT32* volatile Destination = (UINT32*) (ApicVirtualLocalApicBase + offset);
		*Destination = data;
	}

	VOID ApicSendEoi()
	{
		ApicLocalApicWriteRegister(LAPIC_EOI_REGISTER_OFFSET, 0);
	}

	UINT32 ApicLocalApicReadRegister(UINT64 offset)
	{
		UINT32* volatile Source = (UINT32*) (ApicVirtualLocalApicBase + offset);
		return *Source;
	}

	VOID ApicClearError()
	{
		ApicLocalApicWriteRegister(LAPIC_ERROR_REGISTER_OFFSET, 0x00000000);
	}

	VOID ApicSendIpi(UINT8 destination, UINT8 destinationShorthand, UINT8 deliveryMode, UINT8 vector)
	{
		volatile UINT32* icr0 = (volatile UINT32*) (ApicVirtualLocalApicBase + LAPIC_ICR0_REGISTER_OFFSET);
		volatile UINT32* icr1 = (volatile UINT32*) (ApicVirtualLocalApicBase + LAPIC_ICR1_REGISTER_OFFSET);
		*icr0 = (*icr0 & 0x00FFFFFFUL) | (destination << 24);
		*icr1 = (*icr1 & 0xFFF02000UL) | vector | (deliveryMode << 8) | 0x4000 | (destinationShorthand << 18);
	}

	VOID ApicInitIpi(UINT8 destination, UINT8 destinationShorthand)
	{
		ApicSendIpi(destination, destinationShorthand, 5, 0);
	}

	VOID ApicStartupIpi(UINT8 destination, UINT8 destinationShorthand, UINT16 startupCode)
	{
		ApicSendIpi(destination, destinationShorthand, 6, (startupCode & 0xFF000) >> 12);
	}

	UINT8 ApicGetCurrentLapicId()
	{
		return (UINT8) ((ApicLocalApicReadRegister(LAPIC_ID_REGISTER_OFFSET) & 0xFF000000) >> 24);
	}

	VOID ApicSetupTimer(UINT8 interruptVector, ULONG_PTR hundredsOfNsInterval)
	{
		/* make sure the timer is not running */
		ApicLocalApicWriteRegister(LAPIC_INITIAL_TIMER_COUNT_REGISTER_OFFSET, 0x00000000);

		/* set the vector in the LVT entry for the timer */
		ApicLocalApicWriteRegister(LAPIC_LVT_TIMER_REGISTER_OFFSET, interruptVector | LAPIC_TIMER_MODE_PERIODIC);
		ApicLocalApicWriteRegister(LAPIC_DIVIDE_TIMER_REGISTER_OFFSET, LAPIC_TIMER_DIVISOR_1);

		KiCyclesPerClockQuantum = (HalpTimerTicksPerSecond * (hundredsOfNsInterval * 100) / 1000 / 1000 / 1000);
		KiCyclesPerQuantum = KiCyclesPerClockQuantum / 3;
		ApicLocalApicWriteRegister(LAPIC_INITIAL_TIMER_COUNT_REGISTER_OFFSET, (UINT32)KiCyclesPerClockQuantum);
	}

	UINT64 ApicTestTimer()
	{
		/* setup the LAPIC timer divisor */
		ApicLocalApicWriteRegister(LAPIC_DIVIDE_TIMER_REGISTER_OFFSET, LAPIC_TIMER_DIVISOR_1);

		/* wait for 1/20th of a second using PIT and count how many ticks have elapsed on APIC Timer 
		to aproximate the frequency */

		/* arbitrary big number so we have something to substract from */
		const UINT32 initialTicks = 0xFFFFFFFF;

		PitUniprocessorSetupCalibrationSleep();
		ApicLocalApicWriteRegister(LAPIC_INITIAL_TIMER_COUNT_REGISTER_OFFSET, initialTicks);
		PitUniprocessorStartCalibrationSleep();
		
		/* ticksElapsed holds the ticks per 1/20th of a second number */
		UINT64 ticksElapsed = initialTicks - ApicLocalApicReadRegister(LAPIC_CURRENT_TIMER_COUNT_REGISTER_OFFSET);
		UINT64 ticksPerSecond = ticksElapsed * 20;

		/* stop the timer */
		ApicLocalApicWriteRegister(LAPIC_INITIAL_TIMER_COUNT_REGISTER_OFFSET, 0);

		return ticksPerSecond;
	}

	VOID ApicLocalApicInitializeCore()
	{
		ApicLocalApicWriteRegister(LAPIC_SPURIOUS_INTERRUPT_REGISTER_OFFSET, 0x1FF);

		if (!ApicBspLapicInitialized)
		{
			HalpTimerTicksPerSecond = ApicTestTimer();
		}

		ApicSetupTimer(0x20, KeMaximumIncrement);

		if (!ApicBspLapicInitialized)
		{
			ApicBspLapicInitialized = TRUE;
		}

		ApicNumberOfCoresInitialized++;
		KeNumberOfProcessors = ApicNumberOfCoresInitialized;

		ApicLocalApicWriteRegister(LAPIC_DESTINATION_FORMAT_REGISTER, 0xFFFFFFFFUL);
		ApicLocalApicWriteRegister(LAPIC_LOGICAL_DESTINATION_REGISTER, 0xFF000000UL);
	}
}