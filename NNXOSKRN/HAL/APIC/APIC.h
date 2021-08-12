#ifndef NNX_APIC_HEADER
#define NNX_APIC_HEADER

#include "../ACPI/AML.h"

#ifdef __cplusplus
#include "../ACPI/AMLCPP.h"
extern "C" {
#endif
	/* Functions accessible from both C and C++ */

	VOID ApicInit(ACPI_MADT* madt);

#ifdef __cplusplus
}

/* Functions accessible only from C++ */


#endif

#endif