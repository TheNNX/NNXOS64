#include <irql.h>
#include <interrupt.h>
#include <pcr.h>
#include <bugcheck.h>
#include <cpu.h>
#include <ntdebug.h>

static
VOID
KiApplyIrql(
    KIRQL OldValue, 
    KIRQL NewValue)
{
    if (OldValue == NewValue)
    {
        return;
    }
    __writegsbyte(FIELD_OFFSET(KPCR, Irql), NewValue);
    HalSetTpr(NewValue);
}

KIRQL 
FASTCALL 
KfRaiseIrql(
    KIRQL NewIrql)
{
    KIRQL OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

    if (NewIrql < OldIrql)
    {
        KeBugCheckEx(
            IRQL_NOT_GREATER_OR_EQUAL,
            (ULONG_PTR)NewIrql,
            (ULONG_PTR)OldIrql,
            0,
            (ULONG_PTR)_ReturnAddress());
    }

    KiApplyIrql(OldIrql, NewIrql);
    return OldIrql;
}

VOID 
FASTCALL 
KfLowerIrql(
    KIRQL NewIrql)
{
    KIRQL OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));

    if (NewIrql > OldIrql)
    {
        KeBugCheckEx(
            IRQL_NOT_LESS_OR_EQUAL,
            (ULONG_PTR)NewIrql,
            (ULONG_PTR)OldIrql,
            0,
            (ULONG_PTR)_ReturnAddress());
    }

    KiApplyIrql(OldIrql, NewIrql);
}

VOID 
NTAPI 
KeRaiseIrql(
    KIRQL NewIrql, 
    PKIRQL OldIrql)
{
    if (OldIrql == NULL)
    {
        return;
    }

    if (NewIrql < KeGetCurrentIrql())
    {
        KeBugCheckEx(
            IRQL_NOT_GREATER_OR_EQUAL,
            (ULONG_PTR)NewIrql,
            (ULONG_PTR)KeGetCurrentIrql(),
            0,
            (ULONG_PTR)_ReturnAddress());
    }

    *OldIrql = KfRaiseIrql(NewIrql);
}

VOID 
NTAPI 
KeLowerIrql(
    KIRQL OldIrql)
{
    if (OldIrql > KeGetCurrentIrql())
    {
        KeBugCheckEx(
            IRQL_NOT_LESS_OR_EQUAL,
            (ULONG_PTR)OldIrql,
            (ULONG_PTR)KeGetCurrentIrql(),
            0,
            (ULONG_PTR)_ReturnAddress());
    }
    KfLowerIrql(OldIrql);
}

KIRQL 
NTAPI 
KeGetCurrentIrql()
{
    return __readgsbyte(FIELD_OFFSET(KPCR, Irql));
}

KIRQL
KiSetIrql(
    KIRQL NewIrql)
{
    KIRQL OldIrql;

    OldIrql = __readgsbyte(FIELD_OFFSET(KPCR, Irql));
    KiApplyIrql(OldIrql, NewIrql);

    return OldIrql;
}