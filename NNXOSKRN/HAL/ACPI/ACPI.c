#include "AML.h"

UINT8 localLastError = 0;

UINT8 ACPILastError() {
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
		if (!rsdt || !ACPIVerifySDT(rsdt))
		{
			localLastError = ACPI_ERROR_INVALID_RSDT;
			return 0;
		}

		numberOfEntries = (rsdt->Header.Lenght - sizeof(rsdt->Header)) / 4;
	}
	else {
		xsdt = GetXSDT(rdsp);
		if (!xsdt || !ACPIVerifySDT(xsdt))
		{
			localLastError = ACPI_ERROR_INVALID_XSDT;
			return 0;
		}

		numberOfEntries = (xsdt->Header.Lenght - sizeof(xsdt->Header)) / 8;
	}

	for (UINT32 a = 0; a < numberOfEntries; a++) {
		ACPI_SDTHeader* tableAddress = (xsdt == 0) ? ((ACPI_SDTHeader*)rsdt->OtherSDTs[a]) : xsdt->OtherSDTs[a];

		if (*((UINT32*)name) == *((UINT32*)tableAddress->Signature))
			return (ACPI_FADT*)tableAddress;
		else {
			PrintT("%S %S\n", name, 4, tableAddress->Signature, 4);
		}
	}

	localLastError = ACPI_ERROR_SDT_NOT_FOUND;
	return 0;
}

BOOL ACPIVerifySDT(ACPI_SDTHeader* sdt) {
	UINT32 sum = 0;
	for (UINT32 index = 0; index < sdt->Lenght; index++) {
		sum += ((char*)sdt)[index];
	}
	return (sum & 0xFF) == 0;
}


BOOL ACPIVerifyRDSP(ACPI_RDSP* rdsp) {
	UINT32 Lenght = sizeof(ACPI_RDSP) - sizeof(ACPI_RDSPExtension);
	if (rdsp->Revision > 0) {
		Lenght = rdsp->v20.Lenght;
	}

	UINT32 sum = 0;
	for (UINT32 index = 0; index < Lenght; index++) {
		sum += ((char*)rdsp)[index];
	}
	return (sum & 0xFF) == 0;
}