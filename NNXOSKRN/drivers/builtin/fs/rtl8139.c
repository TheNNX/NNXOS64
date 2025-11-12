#include <nnxtype.h>
#include <Port.h>
#include <mm.h>
#include <HALX64\include\PCI.h>
#include <interrupt.h>
#include <HALX64\include\APIC.h>

static
BOOLEAN 
RT8139Isr(PKINTERRUPT pInterrupt,
          PVOID Ctx)
{
    UINT16 ioAddr = (UINT16)(ULONG_PTR)Ctx;

    UINT16 status = inw(ioAddr + 0x3e);
    outw(ioAddr + 0x3E, 0x05);

    PrintT("Networking\n");

    return TRUE;
}

NTSTATUS RT8139Init(UINT32 pciAddress)
{
    BYTE commandByte;
    PFN_NUMBER rxPfnNumber;
    PKINTERRUPT interrupt;

    UINT8 bus = PCI_BUS(pciAddress);
    UINT8 slot = PCI_SLOT(pciAddress);
    UINT8 function = PCI_FUNCTION(pciAddress);
    UINT8 offset = PCI_OFFSET(pciAddress);

    UINT32 bar0;
    UINT16 ioAddr;

    PrintT("Initializing RTL8139\n");

    commandByte = PciConfigReadWord(bus, 
                                    slot,
                                    function, 
                                    offset) & 0xFF;
    commandByte |= 4;

    PciConfigWriteByte(bus, 
                       slot, 
                       function, 
                       offset, commandByte);

    bar0 = PciGetBar0(bus, slot, function);

    ioAddr = bar0 & 0xFFFC;
    outb(ioAddr + 0x52, 0);
    outb(ioAddr + 0x37, 0x10);
    while ((inb(ioAddr + 0x37) & 0x10) != 0) {}

    IoCreateInterrupt(&interrupt,
                      RTL8139_VECTOR,
                      IrqHandler,
                      KeGetCurrentProcessorId(),
                      FALSE,
                      RT8139Isr);

    interrupt->ServiceCtx = (PVOID)(ULONG_PTR)ioAddr;

    KeConnectInterrupt(interrupt);

    MmAllocatePfnRegion(3, &rxPfnNumber, MM_ALLOC_PFN_REGION_4GB);

    /* Init Receive buffer */
    outd(ioAddr + 0x30, (UINT32)PA_FROM_PFN(rxPfnNumber));

    /* Set IMR + ISR */
    outw(ioAddr + 0x3C, 0x0005);

    /* Configuring receive buffer (RCR) */
    outd(ioAddr + 0x44, 0xf | (1 << 7)); // (1 << 7) is the WRAP bit, 0xf is AB+AM+APM+AAP

    /* Enable Receive and Transmitter */
    outb(ioAddr + 0x37, 0x0C);
    //while (1);

    PrintT("MAC: ");
    for (int i = 0; i < 6; i++)
    {
        if (i != 0)
            PrintT("-");
        PrintT("%X", (ULONG_PTR)inb(ioAddr + i));
    }
    PrintT("\n");

    return STATUS_SUCCESS;
}