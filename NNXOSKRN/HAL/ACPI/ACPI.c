#include "AML.h"

UINT8 localLastError = 0;

UINT8 ACPI_LastError() {
	return localLastError;
}

ACPI_RSDT* GetRSDT(ACPI_RDSP* rdsp) {
	if (!ACPIVerifyRDSP(rdsp)) {
		localLastError = ACPI_ERROR_INVALID_RDSP;
		return 0;
	}

	UINT64 result = 0;
	result = rdsp->RSDTAddress;
	return (ACPI_RSDT*)result;
}

ACPI_XSDT* GetXSDT(ACPI_RDSP* rdsp) {
	if (rdsp->Revision == 0)
	{
		localLastError = ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10;
		return 0;
	}
	if (!ACPIVerifyRDSP(rdsp)) {
		localLastError = ACPI_ERROR_INVALID_RDSP;
		return 0;
	}

	return rdsp->v20.XSDTAddress;
}

VOID* GetACPITable(ACPI_RDSP* rdsp, const char* name) {
	UINT32 numberOfEntries;
	ACPI_RSDT* rsdt = 0;
	ACPI_XSDT* xsdt = 0;

	if (rdsp->Revision == 0) {
		rsdt = GetRSDT(rdsp);
		if (!rsdt)
		{
			localLastError = ACPI_ERROR_INVALID_RSDT;
			return 0;
		}
		if (!ACPIVerifyRSDT(rsdt)) {
			localLastError = ACPI_ERROR_INVALID_RSDT;
			return 0;
		}

		UINT32 numberOfEntries = (rsdt->Header.Lenght - sizeof(rsdt->Header)) / 8;
	}
	else {
		xsdt = GetXSDT(rdsp);
		if (!xsdt)
		{
			localLastError = ACPI_ERROR_INVALID_XSDT;
			return 0;
		}
		if (!ACPIVerifyXSDT(xsdt)) {
			localLastError = ACPI_ERROR_INVALID_XSDT;
			return 0;
		}

		numberOfEntries = (xsdt->Header.Lenght - sizeof(xsdt->Header)) / 8;
	}

	for (UINT32 a = 0; a < numberOfEntries; a++) {
		ACPI_SDTHeader* tableAddress = (xsdt == 0) ? ((ACPI_SDTHeader*)rsdt->OtherSDTs[a]) : xsdt->OtherSDTs[a];

		if (*((UINT32*)name) == *((UINT32*)tableAddress->Signature))
		{
			PrintT("%s: %i\n", name, a);
			return (ACPI_FADT*)tableAddress;
		}
		else {
			PrintT("%i: %s\n", a, tableAddress->Signature);
		}
	}

	localLastError = ACPI_ERROR_SDT_NOT_FOUND;
	return 0;
}

ACPI_FADT* GetFADT(ACPI_RDSP* rdsp) {
	ACPI_FADT* result = (ACPI_FADT*)GetACPITable(rdsp, (const UINT8*)"FACP");
	if (result == 0) {
		localLastError = ACPI_ERROR_SDT_NOT_FOUND;
		return 0;
	}

	if (!ACPIVerifyFADT(result)) {
		localLastError = ACPI_ERROR_INVALID_FADT;
		return 0;
	}

	return result;
}

BOOL ACPIVerifyFADT(ACPI_FADT* fadt) {
	UINT32 sum = 0;
	for (UINT32 index = 0; index < fadt->Header.Lenght; index++) {
		sum += ((char*)fadt)[index];
	}
	return (sum & 0xFF) == 0;
}

BOOL ACPIVerifyDSDT(ACPI_DSDT* dsdt) {
	PrintT("Verifying DSDT 0x%X\n",dsdt);
	UINT32 sum = 0;
	for (UINT32 index = 0; index < dsdt->Header.Lenght; index++) {
		sum += ((char*)dsdt)[index];
	}
	return (sum & 0xFF) == 0;
}

BOOL ACPIVerifyXSDT(ACPI_XSDT* xsdt) {
	UINT32 sum = 0;
	for (UINT32 index = 0; index < xsdt->Header.Lenght; index++) {
		sum += ((char*)xsdt)[index];
	}
	return (sum & 0xFF) == 0;
}

BOOL ACPIVerifyRSDT(ACPI_RSDT* rsdt) {
	UINT32 Lenght = rsdt->Header.Lenght;
	UINT64 sum = 0;
	for (UINT32 a = 0; a < Lenght; a++) {
		sum += ((UINT8*)rsdt)[a];
	}
	return (sum & 0xFF) == 0;
}

BOOL ACPIVerifyRDSP(ACPI_RDSP* rdsp) {
	UINT32 Lenght = sizeof(ACPI_RDSP) - sizeof(ACPI_RDSPExtension);
	if (rdsp->Revision > 0) {
		Lenght = rdsp->v20.Lenght;
	}
	PrintT("%x %i %x\n", rdsp, rdsp->Revision, Lenght);
	UINT32 sum = 0;
	for (UINT32 index = 0; index < Lenght; index++) {
		sum += ((char*)rdsp)[index];
	}
	return (sum & 0xFF) == 0;
}
