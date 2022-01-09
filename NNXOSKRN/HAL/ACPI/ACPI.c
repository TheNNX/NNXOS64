#include "AML.h"

UINT8 localLastError = 0;

ACPI_XSDT* gXsdt;
ACPI_RSDT* gRsdt;

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

	if (gRsdt == NULL)
	{
		gRsdt = (ACPI_RSDT*)PagingMapStrcutureToVirtual(rdsp->RSDTAddress, sizeof(ACPI_RSDT), PAGE_PRESENT | PAGE_WRITE);
		PrintT("Allocated RSDT %X\n", gRsdt);
	}
	return gRsdt;
}

ACPI_XSDT* GetXsdt(ACPI_RDSP* rdsp)
{
	if (!AcpiVerifyRdsp(rdsp))
	{
		localLastError = ACPI_ERROR_INVALID_RDSP;
		return 0;
	}
	if (rdsp->Revision == 0)
	{
		localLastError = ACPI_ERROR_NOT_SUPPORTED_BY_ACPI_10;
		return 0;
	}
	if (gXsdt == NULL)
	{
		gXsdt = (ACPI_XSDT*)PagingMapStrcutureToVirtual((ULONG_PTR)rdsp->v20.XSDTAddress, sizeof(ACPI_XSDT), PAGE_PRESENT | PAGE_WRITE);
		PrintT("Allocated XSDT %X\n", gXsdt);
	}

	return gXsdt;
}

VOID* AcpiGetTable(ACPI_RDSP* rdsp, const char* name)
{
	UINT32 numberOfEntries;
	ACPI_RSDT* rsdt = 0;
	ACPI_XSDT* xsdt = 0;
	SIZE_T a;

	if (rdsp->Revision == 0)
	{
		rsdt = GetRsdt(rdsp);
		
		if (!rsdt || !AcpiVerifySdt((ACPI_SDT_HEADER*)rsdt))
		{
			localLastError = ACPI_ERROR_INVALID_RSDT;
			return 0;
		}

		numberOfEntries = (rsdt->Header.Lenght - sizeof(rsdt->Header)) / 4;
	}
	else
	{
		xsdt = GetXsdt(rdsp);

		if (!xsdt || !AcpiVerifySdt((ACPI_SDT_HEADER*)xsdt))
		{
			localLastError = ACPI_ERROR_INVALID_XSDT;
			return 0;
		}

		numberOfEntries = (xsdt->Header.Lenght - sizeof(xsdt->Header)) / 8;
	}

	for (a = 0; a < numberOfEntries; a++)
	{
		ACPI_SDT_HEADER* tableAddress = (xsdt == 0) ? ((ACPI_SDT_HEADER*) (ULONG_PTR)rsdt->OtherSDTs[a]) : xsdt->OtherSDTs[a];

		if (*((UINT32*) name) == *((UINT32*) tableAddress->Signature))
			return (ACPI_FADT*) tableAddress;
	}

	localLastError = ACPI_ERROR_SDT_NOT_FOUND;
	return 0;
}

UINT8 SumBytes(UINT8* src, SIZE_T len)
{
	UINT8 sum = 0;
	UINT32 index;

	for (index = 0; index < len; index++)
	{
		sum += (src)[index];
	}

	return sum;
}

BOOL AcpiVerifySdt(ACPI_SDT_HEADER* sdt)
{
	return SumBytes((UINT8*)sdt, sdt->Lenght) == 0;
}


BOOL AcpiVerifyRdsp(ACPI_RDSP* rdsp)
{
	UINT32 Lenght = sizeof(ACPI_RDSP) - sizeof(ACPI_RDSPExtension);
	if (rdsp->Revision > 0)
	{
		Lenght = rdsp->v20.Lenght;
	}

	return SumBytes((UINT8*)rdsp, Lenght) == 0;
}