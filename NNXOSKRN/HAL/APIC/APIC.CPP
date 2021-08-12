#include "APIC.h"

UINT64 ApicLocalApicBase;
UINT64 ApicNumberOfCoresDetected;
BYTE* ApicLocalApicIDs;

extern "C" {
	VOID ApicInit(ACPI_MADT* madt) {
		UINT64 i = 0, currentLocalApicIdIndex = 0;
		UINT8* currentPointer = (UINT8*)&(madt->InteruptControlerStruct);

		ApicLocalApicBase = madt->LAPICBase32;

		for (i = 0; i < madt->Header.Lenght - sizeof(ACPI_MADT); ) {
			ACPI_MADT_ENTRY* madtEntry = ((ACPI_MADT_ENTRY*)(currentPointer + i));
			UINT8 type = madtEntry->Type;

			if (type == ACPI_MADT_TYPE_LAPIC) {
				ACPI_MADT_LAPIC* lapic = (ACPI_MADT_LAPIC*)madtEntry;
				ApicNumberOfCoresDetected += (lapic->Flags & 1) ? 1 : 0;
			}
			else if (type == ACPI_MADT_TYPE_IOAPIC) {
				ACPI_MADT_IOAPIC* ioapic = (ACPI_MADT_IOAPIC*)madtEntry;
				PrintT("IOAPIC %i[%x] starting at global INT %x\n", ioapic->IoApicID, ioapic->IoApicAddress, ioapic->IoApicInterruptBase);
			}
			else if (type == ACPI_MADT_TYPE_SOURCE_OVERRIDE) {
				ACPI_MADT_SOURCE_OVERRIDE* sourceOverride = (ACPI_MADT_SOURCE_OVERRIDE*)madtEntry;
				PrintT("Interrupt mapping: %x.%x -> %x, flags %x\n", sourceOverride->Bus, sourceOverride->Source, sourceOverride->GlobalSystemInterrupt, (UINT64)sourceOverride->Flags);
			}
			else if (type == ACPI_MADT_TYPE_IOAPIC_NMI_SOURCE) {
				ACPI_MADT_IOAPIC_NMI_SOURCE* ioApicNmi = (ACPI_MADT_IOAPIC_NMI_SOURCE*)madtEntry;
				PrintT("Global interrupt %x is NMI\n", ioApicNmi->GlobalSystemInterrupt);
			}
			else if (type == ACPI_MADT_TYPE_LAPIC_NMI_SOURCE) {
				ACPI_MADT_LAPIC_NMI_SOURCE* lApicNmi = (ACPI_MADT_LAPIC_NMI_SOURCE*)madtEntry;
				PrintT("Processor %x LINT%i is NMI\n", lApicNmi->ProcessorUID, lApicNmi->LapicLintN);
			}
			else if (type == ACPI_MADT_TYPE_LAPIC_ADDRESS_64) {
				ACPI_MADT_LAPIC_ADDRESS_64* lapicOverride = (ACPI_MADT_LAPIC_ADDRESS_64*)madtEntry;
				ApicLocalApicBase = lapicOverride->AddressOverride;
				PrintT("LAPIC address override present: 0x%X\n", ApicLocalApicBase);
			}

			i += madtEntry->Length;
		}

		ApicLocalApicIDs = (BYTE*)NNXAllocatorAllocArray(ApicNumberOfCoresDetected, sizeof(BYTE));

		for (i = 0; i < madt->Header.Lenght - sizeof(ACPI_MADT); ) {
			ACPI_MADT_ENTRY* madtEntry = ((ACPI_MADT_ENTRY*)(currentPointer + i));
			UINT8 type = madtEntry->Type;

			if (type == ACPI_MADT_TYPE_LAPIC) {
				ACPI_MADT_LAPIC* lapic = (ACPI_MADT_LAPIC*)madtEntry;
				if (lapic->Flags & 1) {
					PrintT("LAPIC %i for UID %i%s\n", lapic->LapicID, lapic->ProcessorUID, (lapic->Flags % 1) ? (" [disabled]") : (""));
					ApicLocalApicIDs[currentLocalApicIdIndex++] = lapic->LapicID;
				}
			}

			i += madtEntry->Length;
		}
	}
}