#include <nnxtype.h>
#include <intrin.h>
#include <HAL/X64/IDT.h>
#include <HAL/interrupt.h>

VOID
NTAPI
HalpInitializeIdt(
    PKIDTENTRY64 Idt,
    PKIDTR64 Idtr)
{
    Idtr->Size = sizeof(KIDTENTRY64) * 256 - 1;
    Idtr->Base = Idt;

    for (int a = 0; a < 256; a++)
    {
        KIDTENTRY64 result;
        VOID(*handler)();

        handler = HalpDefInterruptHandler;
        result = HalBindInterrupt(Idt, a, handler, FALSE, FALSE);
    }

    __lidt(Idtr);
}

KIDTENTRY64
NTAPI
HalBindInterrupt(
    KIDTENTRY64* Idt,
    UINT64 EntryNo,
    PVOID Handler,
    BOOL Usermode,
    BOOL Trap)
{
    KIDTENTRY64 oldEntry = Idt[EntryNo];

    Idt[EntryNo].Selector = 0x8;
    Idt[EntryNo].Zero = 0;
    Idt[EntryNo].Offset0to15 =
        (UINT16)(((ULONG_PTR)Handler) & UINT16_MAX);
    Idt[EntryNo].Offset16to31 =
        (UINT16)((((ULONG_PTR)Handler) >> 16) & UINT16_MAX);
    Idt[EntryNo].Offset32to63 =
        (UINT32)((((ULONG_PTR)Handler) >> 32) & UINT32_MAX);
    Idt[EntryNo].Type = 0x8E | (Usermode ? (0x60) : 0x00) | (Trap ? 0 : 1);
    Idt[EntryNo].Ist = 0;

    return oldEntry;
}

VOID
NTAPI
HalpInitInterruptHandlerStub(
    PKINTERRUPT pInterrupt,
    ULONG_PTR ProperHandler)
{
    SIZE_T idx = 0;

    /* push rax */
    pInterrupt->Handler[idx++] = 0x50;
    /* mov rax, ProperHandler */
    pInterrupt->Handler[idx++] = 0x48;
    pInterrupt->Handler[idx++] = 0xB8;
    *((ULONG_PTR*)&pInterrupt->Handler[idx]) = ProperHandler;
    idx += sizeof(ULONG_PTR);
    /* push rax */
    pInterrupt->Handler[idx++] = 0x50;
    /* mov rax, pInterrupt */
    pInterrupt->Handler[idx++] = 0x48;
    pInterrupt->Handler[idx++] = 0xB8;
    *((PKINTERRUPT*)&pInterrupt->Handler[idx]) = pInterrupt;
    idx += sizeof(ULONG_PTR);
    /* ret */
    pInterrupt->Handler[idx++] = 0xC3;
}