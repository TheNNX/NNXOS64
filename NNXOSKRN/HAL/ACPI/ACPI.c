#include "AML.h"

UINT8 localLastError = 0;

UINT8 AcpiLastError()
{
	return localLastError;
}

ACPI_RSDT* GetRsdt(ACPI_RDSP* rdsp)
{
	if (!AcpiVerifyRdsp(rdsp))
	{
		localLastError = ACPI_ERROR_INVALID_RDSP;
		return 0;
	}

	UINT64 result = 0;
	result = rdsp->RSDTAddress;
	return (ACPI_RSDT*) result;
}

ACPI_XSDT* GetXsdt(ACPI_RDSP* rdsp)
{
	if (rdsp->Revision == 0)
	{
		localLastError = ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10;
		return 0;
	}
	if (!AcpiVerifyRdsp(rdsp))
	{
		localLastError = ACPI_ERROR_INVALID_RDSP;
		return 0;
	}

	return rdsp->v20.XSDTAddress;
}

VOID* AcpiGetTable(ACPI_RDSP* rdsp, const char* name)
{
	UINT32 numberOfEntries;
	ACPI_RSDT* rsdt = 0;
	ACPI_XSDT* xsdt = 0;

	if (rdsp->Revision == 0)
	{
		rsdt = GetRsdt(rdsp);
		if (!rsdt || !AcpiVerifySdt(rsdt))
		{
			localLastError = ACPI_ERROR_INVALID_RSDT;
			return 0;
		}

		numberOfEntries = (rsdt->Header.Lenght - sizeof(rsdt->Header)) / 4;
	}
	else
	{
		xsdt = GetXsdt(rdsp);
		if (!xsdt || !AcpiVerifySdt(xsdt))
		{
			localLastError = ACPI_ERROR_INVALID_XSDT;
			return 0;
		}

		numberOfEntries = (xsdt->Header.Lenght - sizeof(xsdt->Header)) / 8;
	}

	for (UINT32 a = 0; a < numberOfEntries; a++)
	{
		ACPI_SDT_HEADER* tableAddress = (xsdt == 0) ? ((ACPI_SDT_HEADER*) rsdt->OtherSDTs[a]) : xsdt->OtherSDTs[a];

		if (*((UINT32*) name) == *((UINT32*) tableAddress->Signature))
			return (ACPI_FADT*) tableAddress;
		else
		{
			PrintT("not matched %S %S\n", name, 4LL, tableAddress->Signature, 4LL);
		}
	}

	localLastError = ACPI_ERROR_SDT_NOT_FOUND;
	return 0;
}

BOOL AcpiVerifySdt(ACPI_SDT_HEADER* sdt)
{
	UINT32 sum = 0;
	for (UINT32 index = 0; index < sdt->Lenght; index++)
	{
		sum += ((char*) sdt)[index];
	}
	return (sum & 0xFF) == 0;
}


BOOL AcpiVerifyRdsp(ACPI_RDSP* rdsp)
{
	UINT32 Lenght = sizeof(ACPI_RDSP) - sizeof(ACPI_RDSPExtension);
	if (rdsp->Revision > 0)
	{
		Lenght = rdsp->v20.Lenght;
	}

	UINT32 sum = 0;
	for (UINT32 index = 0; index < Lenght; index++)
	{
		sum += ((char*) rdsp)[index];
	}
	return (sum & 0xFF) == 0;
}