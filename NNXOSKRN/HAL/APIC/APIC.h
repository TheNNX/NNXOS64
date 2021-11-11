#ifndef NNX_APIC_HEADER
#define NNX_APIC_HEADER


#include "../ACPI/AML.h"

#ifdef __cplusplus
#include "../ACPI/AMLCPP.h"
extern "C" {
#endif
	/* Functions accessible from both C and C++ */
	VOID ApicInit(ACPI_MADT* madt);
	VOID ApicLocalApicWriteRegister(UINT64 offset, UINT32 data);
	UINT32 ApicLocalApicReadRegister(UINT64 offset);
	VOID ApicClearError();
	VOID ApicSendIpi(UINT8 destination, UINT8 destinationShorthand, UINT8 deliveryMode, UINT8 vector);
	VOID ApicInitIpi(UINT8 destination, UINT8 destinationShorthand);
	VOID ApicStartupIpi(UINT8 destination, UINT8 destinationShorthand, UINT16 startupCode);
	VOID ApicLocalApicWriteRegister(UINT64 offset, UINT32 data);
	UINT32 ApicLocalApicReadRegister(UINT64 offset);
	UINT8 ApicGetCurrentLapicId();
	extern UINT64 ApicNumberOfCoresDetected;
	extern UINT8* ApicLocalApicIDs;
	extern UINT64 ApicVirtualLocalApicBase;
	extern UINT64 ApicLocalApicBase;
#ifdef __cplusplus
}

#endif

/* Constants */
#define LAPIC_ID_REGISTER_OFFSET					0x20
#define LAPIC_TASK_PRIORITY_REGISTER_OFFSET			0x80
#define LAPIC_SPURIOUS_INTERRUPT_REGISTER_OFFSET	0xF0
#define LAPIC_ERROR_REGISTER_OFFSET					0x280
#define	LAPIC_ICR1_REGISTER_OFFSET					0x300
#define LAPIC_ICR0_REGISTER_OFFSET					0x310
#define LAPIC_LVT_LINT0_REGISTER_OFFSET				0x350
#define LAPIC_LVT_LINT1_REGISTER_OFFSET				0x360
#define LAPIC_LVT_ERROR_REGISTER_OFFSET				0x370

#endif