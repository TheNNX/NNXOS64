#include "APIC.h"
#include "../../../NNXOSLDR/HAL/PIC.h"
#include "../../../NNXOSLDR/memory/paging.h"
#include "../../../NNXOSLDR/memory/physical_allocator.h"
#include "../../../NNXOSLDR/nnxlog.h"
#include "../../../NNXOSLDR/device/fs/fat.h"
#include "../../../NNXOSLDR/HAL/PIT.h"
#include "../spinlock.h"

UINT64 ApicLocalApicBase;
UINT64 ApicVirtualLocalApicBase;
UINT64 ApicNumberOfCoresDetected;
UINT8* ApicLocalApicIDs;

class IoApic
{

/* public functions */
public:
	IoApic(volatile UINT32* base, UINT32 interruptBase)
	{
		PrintT("IoApicBase: %x\n", base);

		KeInitializeSpinLock(&Lock);
		this->IoApicBasePhys = base;
		this->InterruptBase = interruptBase;
		
		this->IoApicBase = (volatile UINT32*)PagingAllocatePageBlockWithPhysicalAdresses(
			2,
			PAGING_KERNEL_SPACE,
			PAGING_KERNEL_SPACE_END,
			PAGE_WRITE | PAGE_PRESENT | PAGE_NO_CACHE,
			(PVOID) (((UINT64) IoApicBasePhys) & PAGE_SIZE_SMALL)
		);
		
		this->IoApicBase = (volatile UINT32*) (((UINT64)this->IoApicBase) + (((UINT64)IoApicBasePhys) & PAGE_SIZE_SMALL));
		this->Id = (ReadRegister(0x00) >> 24) & 0xF0;
		this->RedirectionEntriesCount = (ReadRegister(0x01) >> 16) + 1;
	}

	BYTE GetID()
	{
		return this->Id;
	}

	BOOL MaskIrq(UINT32 irqn)
	{
		if (!this->InterruptRangeCheck(irqn))
			return FALSE;

		KeAcquireSpinLock(&this->Lock, &irql);

		irqn -= this->InterruptBase;

		UINT64 entry = ReadRedirectionEntry(irqn);
		entry |= ((1ULL << 16ULL));

		KeReleaseSpinLock(&this->Lock, irql);

		return TRUE;
	}

	BOOL UnmaskIrq(UINT32 irqn)
	{
		if (!this->InterruptRangeCheck(irqn))
			return FALSE;

		KeAcquireSpinLock(&this->Lock, &irql);

		irqn -= this->InterruptBase;

		UINT64 entry = ReadRedirectionEntry(irqn);
		entry &= ~((1ULL << 16ULL));

		KeReleaseSpinLock(&this->Lock, irql);

		return TRUE;
	}

	BOOL InterruptRangeCheck(UINT32 irqn)
	{
		if (irqn < this->InterruptBase)
			return FALSE;

		irqn -= this->InterruptBase;

		if (irqn >= this->RedirectionEntriesCount)
			return FALSE;

		return TRUE;
	}
/* public variables */
public:
	NNXLogger* logger = NULL;
/* private functions */
private:
	VOID WriteRegister(const UINT8 offset, const UINT32 value)
	{
		*((volatile UINT32*) (IoApicBase)) = offset;
		*((volatile UINT32*) (IoApicBase + 0x10)) = value;
	}

	UINT32 ReadRegister(const UINT8 offset)
	{
		*((volatile UINT32*) (IoApicBase)) = offset;
		return *((volatile UINT32*) (IoApicBase + 0x10));
	}

	UINT64 ReadRedirectionEntry(const UINT32 irqNumber)
	{
		DWORD lowDword, highDword;
		lowDword = ReadRegister(0x10 + irqNumber * 2);
		highDword = ReadRegister(0x10 + irqNumber * 2 + 1);

		return (((UINT64)lowDword) | (((UINT64)highDword) << 32ULL));
	}

	VOID WriteRedirectionEntry(const UINT32 irqNumber, UINT64 entry)
	{
		WriteRegister(0x10 + irqNumber * 2, entry & 0xFFFFFFFFUL);
		WriteRegister(0x10 + irqNumber * 2 + 1, (entry >> 32ULL) & 0xFFFFFFFFUL);
	}

/* private variables */
private:
	UINT32 InterruptBase;
	UINT32 RedirectionEntriesCount;
	volatile UINT32* IoApicBasePhys;
	volatile UINT32* IoApicBase;
	BYTE Id;
	KSPIN_LOCK Lock = { 0 };
	KIRQL irql = 0;
};

IoApic** IoApics;
UINT64 IoApicCount;

extern "C"
{
	extern BOOL HalpInteruptInitialized;

	VOID ApicInit(ACPI_MADT* madt)
	{
		UINT64 i = 0, currentLocalApicIdIndex = 0, currentIoApicIndex = 0;
		UINT8* currentPointer = (UINT8*)&(madt->InteruptControlerStruct);
		NNXLogger* logger = new NNXLogger(VfsGetPointerToVfs(0)->Functions.OpenOrCreateFile(VfsGetPointerToVfs(0), (char*)"APICLOG.TXT"));

		logger->Log("LAPIC initialization started\n");

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
				ACPI_MADT_IOAPIC* ioapic = (ACPI_MADT_IOAPIC*) madtEntry;
				logger->Log("Starting IOAPIC initialization\n");
				IoApics[currentIoApicIndex++] = new IoApic((volatile UINT32*)ioapic->IoApicAddress, ioapic->IoApicInterruptBase);
				
			}

			i += madtEntry->Length;
		}

		PicDisableForApic();

		HalpInteruptInitialized = TRUE;

		/* find virtual address space for the APIC registers */
		ApicVirtualLocalApicBase = (UINT64) PagingAllocatePageWithPhysicalAddress2(
			PAGING_KERNEL_SPACE, PAGING_KERNEL_SPACE_END, 
			PAGE_PRESENT | PAGE_WRITE, (VOID*) PAGE_ALIGN(ApicLocalApicBase)
		);
		ApicVirtualLocalApicBase += (ApicLocalApicBase % PAGE_SIZE_SMALL);

		/* ensure the physical page is marked as used in the memory map */
		GlobalPhysicalMemoryMap[ApicLocalApicBase / PAGE_SIZE_SMALL] = MEM_TYPE_USED_PERM;

		/* enables the LAPIC */
		ApicLocalApicWriteRegister(LAPIC_SPURIOUS_INTERRUPT_REGISTER_OFFSET, 0x1FF);
	}

	VOID ApicLocalApicWriteRegister(UINT64 offset, UINT32 data)
	{
		UINT32* volatile Destination = (UINT32* volatile) (ApicVirtualLocalApicBase + offset);
		*Destination = data;
	}

	UINT32 ApicLocalApicReadRegister(UINT64 offset)
	{
		UINT32* volatile Source = (UINT32* volatile) (ApicVirtualLocalApicBase + offset);
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
		return (UINT8) (ApicLocalApicReadRegister(LAPIC_ID_REGISTER_OFFSET) & 0xFF);
	}
}