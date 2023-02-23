#include "APIC.h"

IoApic::IoApic(volatile UINT32* base, UINT32 interruptBase)
{
	KeInitializeSpinLock(&Lock);
	this->IoApicBasePhys = base;
	this->InterruptBase = interruptBase;

	this->IoApicBase = (volatile UINT32*)PagingAllocatePageBlockWithPhysicalAddresses(
		2,
		PAGING_KERNEL_SPACE,
		PAGING_KERNEL_SPACE_END,
		PAGE_WRITE | PAGE_PRESENT | PAGE_NO_CACHE,
		((ULONG_PTR)IoApicBasePhys) & PAGE_ADDRESS_MASK
	);

	this->IoApicBase = (volatile UINT32*)(((UINT64)this->IoApicBase) + (((UINT64)IoApicBasePhys) & PAGE_FLAGS_MASK));
	this->Id = (ReadRegister(0x00) >> 24) & 0xF0;
	this->RedirectionEntriesCount = (ReadRegister(0x01) >> 16) + 1;

	/* mask all external interrupts */
	for (UINT32 i = 0; i < this->RedirectionEntriesCount; i++)
	{
		MaskIrq(this->InterruptBase + i);
	}
}

BYTE IoApic::GetID()
{
	return this->Id;
}

BOOL IoApic::MaskIrq(UINT32 irqn)
{
	if (!this->InterruptRangeCheck(irqn))
		return FALSE;

	irqn -= this->InterruptBase;

	UINT64 entry = ReadRedirectionEntry(irqn);
	entry |= ((1ULL << 16ULL));
	WriteRedirectionEntry(irqn, entry);
	return TRUE;
}

BOOL IoApic::UnmaskIrq(UINT32 irqn)
{
	if (!this->InterruptRangeCheck(irqn))
		return FALSE;

	irqn -= this->InterruptBase;

	UINT64 entry = ReadRedirectionEntry(irqn);
	entry &= ~((1ULL << 16ULL));
	WriteRedirectionEntry(irqn, entry);
	return TRUE;
}

BOOL IoApic::InterruptRangeCheck(UINT32 irqn)
{
	if (irqn < this->InterruptBase)
		return FALSE;

	irqn -= this->InterruptBase;

	if (irqn >= this->RedirectionEntriesCount)
		return FALSE;

	return TRUE;
}

VOID IoApic::WriteRegister(const UINT8 offset, const UINT32 value)
{
	HalAcquireLockRaw(&this->Lock);
	*((volatile UINT32*)(IoApicBase)) = offset;
	*((volatile UINT32*)(((ULONG_PTR)IoApicBase) + 0x10)) = value;
	HalReleaseLockRaw(&this->Lock);
}

UINT32 IoApic::ReadRegister(const UINT8 offset)
{
	UINT32 result;

	HalAcquireLockRaw(&this->Lock);
	*((volatile UINT32*)(IoApicBase)) = offset;
	result = *((volatile UINT32*)(((ULONG_PTR)IoApicBase) + 0x10));
	HalReleaseLockRaw(&this->Lock);

	return result;
}

UINT64 IoApic::ReadRedirectionEntry(const UINT32 irqNumber)
{
	DWORD lowDword, highDword;

	lowDword = ReadRegister(0x10 + irqNumber * 2);
	highDword = ReadRegister(0x10 + irqNumber * 2 + 1);

	return (((UINT64)lowDword) | (((UINT64)highDword) << 32ULL));
}

VOID IoApic::WriteRedirectionEntry(const UINT32 irqNumber, UINT64 entry)
{
	WriteRegister(0x10 + irqNumber * 2, entry & 0xFFFFFFFFUL);
	WriteRegister(0x10 + irqNumber * 2 + 1, (entry >> 32ULL) & 0xFFFFFFFFUL);
}
