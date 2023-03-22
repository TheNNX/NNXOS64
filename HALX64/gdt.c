#include <HAL/X64/GDT.h>
#include <HAL/paging.h>

/* Declarations */

static USHORT HalpSetGdtEntry(
	LPKGDTENTRY64 gdt,
	USHORT entryIndex,
	UINT32 base,
	UINT32 limit,
	UINT8 flags,
	UINT8 accessByte);

static USHORT HalpSetGdtTssDescriptorEntry(
	LPKGDTENTRY64 gdt,
	USHORT entryIndex,
	PVOID tss,
	SIZE_T tssSize);

/* Definitions */

VOID
NTAPI
HalpInitializeGdt(
	PKGDTENTRY64 Gdt, 
	KGDTR64* Gdtr,
	PKTSS Tss)
{
	USHORT null, code0, data0, code3, data3, tr;
	Gdtr->Size = sizeof(KGDTENTRY64) * 7 - 1;
	Gdtr->Base = Gdt;

	Tss->IopbBase = sizeof(*Tss);

	/* For magic numbers meaning, 
	 * see section 3.4.5 of Intel Software Developer’s Manual Volume 3 
	 * and https://wiki.osdev.org/Global_Descriptor_Table */
	null = HalpSetGdtEntry(Gdt, 0, 0x00000000UL, 0x00000UL, 0x0, 0x00);
	code0 = HalpSetGdtEntry(Gdt, 1, 0x00000000UL, 0xFFFFFUL, 0xA, 0x9A);
	data0 = HalpSetGdtEntry(Gdt, 2, 0x00000000UL, 0xFFFFFUL, 0xC, 0x92);
	data3 = HalpSetGdtEntry(Gdt, 3, 0x00000000UL, 0xFFFFFUL, 0xC, 0xF2);
	code3 = HalpSetGdtEntry(Gdt, 4, 0x00000000UL, 0xFFFFFUL, 0xA, 0xFA);
	tr = HalpSetGdtTssDescriptorEntry(Gdt, 5, Tss, sizeof(*Tss));

	HalpLoadGdt(Gdtr);
	HalpLoadTss(tr);
}

static 
USHORT 
HalpSetGdtEntry(
	LPKGDTENTRY64 gdt,
	USHORT entryIndex,
	UINT32 base, 
	UINT32 limit,
	UINT8 flags, 
	UINT8 accessByte)
{
	gdt[entryIndex].Base0To15 = base & UINT16_MAX;
	gdt[entryIndex].Base16To23 = (base & 0xFF0000) >> 16;
	gdt[entryIndex].Base24To31 = (base & 0xFF000000) >> 24;
	gdt[entryIndex].Limit0To15 = limit & UINT16_MAX;
	gdt[entryIndex].Limit16To19 = (limit & 0xF0000) >> 16;
	gdt[entryIndex].Flags = flags & 0xF;
	gdt[entryIndex].AccessByte = accessByte;

	return sizeof(KGDTENTRY64) * entryIndex;
}

static 
USHORT 
HalpSetGdtTssDescriptorEntry(
	LPKGDTENTRY64 gdt, 
	USHORT entryIndex, 
	PVOID tss,
	SIZE_T tssSize
)
{
	HalpSetGdtEntry(gdt, entryIndex, ((ULONG_PTR)tss) & UINT32_MAX, (UINT32)tssSize, 0x40, 0x89);
	*((UINT64*)(gdt + entryIndex + 1)) = (((ULONG_PTR)tss) & ~((UINT64)UINT32_MAX)) >> 32;

	return sizeof(KGDTENTRY64) * entryIndex;
}

ULONG 
NTAPI
HalpGetGdtBase(
	KGDTENTRY64 entry)
{
	return entry.Base0To15 | 
			(entry.Base16To23 << 16UL) |
			(entry.Base24To31 << 24UL);
}

PKTSS 
NTAPI
HalpGetTssBase(
	KGDTENTRY64 tssEntryLow, 
	KGDTENTRY64 tssEntryHigh)
{
	return (PKTSS)(HalpGetGdtBase(tssEntryLow) | ((*((ULONG_PTR*)&tssEntryHigh)) << 32UL));
}

/**
 * @brief Finds a GDT entry matching the conditions and returns its selector.
 */
USHORT 
NTAPI
HalpGdtFindEntry(
	LPKGDTENTRY64 gdt,
	USHORT numberOfEntries,
	BOOL code, 
	BOOL user)
{
	USHORT i;

	/* iterate from 1 to skip the null descriptor */
	for (i = 1; i < numberOfEntries; i++)
	{
		LPKGDTENTRY64 currentEntry = gdt + i;
		USHORT ab = currentEntry->AccessByte;

		/* if this is a TSS entry, skip the next one (it is the second half of this one) */
		if ((ab & 0xF) == 0x9 ||
			(ab & 0xF) == 0xB)
		{
			i++;
		}
		
		if (code && !user && ab == 0x9A) return i * sizeof(KGDTENTRY64);
		if (code && user && ab == 0xFA) return (i * sizeof(KGDTENTRY64)) | 3;
		if (!code && !user && ab == 0x92) return i * sizeof(KGDTENTRY64);
		if (!code && user && ab == 0xF2) return (i * sizeof(KGDTENTRY64)) | 3;
	}

	return 0;
}
