#include <HALX64/include/PCIIDE.h>
#include <HALX64/include/PCI.h>
#include <SimpleTextIo.h>
#include <Port.h>
#include <mbr.h>
#include <gpt.h>

#pragma warning(disable : 4189)

NTSTATUS RT8139Init(UINT32 pciAddress);
void PciBridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
void PciMassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
VOID PciIdeEnumerate();

UINT16 PciConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset)
{
    UINT32 address = PCI_ADDRESS(bus, slot, function, offset);
    outd(CONFIG_ADDRESS, address);
    return (UINT16) ((ind(CONFIG_DATA) >> ((offset & 3) * 8)) & 0xffff);
}

UINT32 PciConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset)
{
    UINT32 address = PCI_ADDRESS(bus, slot, function, offset);
    outd(CONFIG_ADDRESS, address);
    return ind(CONFIG_DATA) >> ((offset & 2) * 8);
}

VOID PciConfigWriteByte(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset, UINT8 byte)
{
    UINT32 address = PCI_ADDRESS(bus, slot, function, offset);
    outd(CONFIG_ADDRESS, address);
    outb(CONFIG_DATA, byte);
}


VOID PciScan()
{
    UINT8 header = PciGetHeader(0, 0, 0);
    if (header & 0x80)
    {
        PciScanBus(0);
    }
    else
    {
        for (UINT8 function = 0;; function++)
        {
            UINT8 bus = function;
            PciScanBus(bus);
            if (function == 0xff)
                break;
        }
    }

    PciIdeEnumerate();
}

void PciScanBus(UINT8 busNumber)
{
    if (!PciCheckIfPresent(0, 0, busNumber))
    {
        return;
    }
    
    for (UINT8 device = 0;; device++)
    {
        PciScanDevice(busNumber, device);
        if (device == 0xff)
        {
            return;
        }
    }
}

void PciScanDevice(UINT8 busNumber, UINT8 deviceNumber)
{
    UINT8 header;

    if (!PciCheckIfPresent(busNumber, deviceNumber, 0))
    {
        return;
    }

    PrintT("Scanning device %X:%X\n", busNumber, deviceNumber);
    PciScanFunction(busNumber, deviceNumber, 0);
    
    header = PciGetHeader(busNumber, deviceNumber, 0);
    if (header & 0x80)
    {
        for (UINT8 function = 1; function < 8; function++)
        {
            PciScanFunction(busNumber, deviceNumber, function);
        }
    }
}

void PciScanFunction(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber)
{
    if (!PciCheckIfPresent(busNumber, deviceNumber, functionNumber))
        return;
    
    UINT8 Class = PCIGetClass(busNumber, deviceNumber, functionNumber);
    UINT8 Subclass = PciGetSubclass(busNumber, deviceNumber, functionNumber);
    
    PrintT("Checking function %X:%X:%X - %X:%X\n", busNumber, deviceNumber, functionNumber, Class, Subclass);
    
    switch (Class)
    {
        case PCI_CLASS_BRIDGE_DEVICE:
            PciBridgeDeviceClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
            break;
        case PCI_CLASS_MASS_STORAGE_CONTROLLER:
            PciMassStorageClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
            break;
        case PCI_CLASS_NETWORK_CONTROLLER:
            if (PCIGetVendorId(busNumber, deviceNumber, functionNumber) == 0x10EC &&
                PCIGetDeviceId(busNumber, deviceNumber, functionNumber) == 0x8139)
            {
                RT8139Init(PCI_ADDRESS(busNumber, deviceNumber, functionNumber, 0));
            }
            else
            {
                PrintT("Unrecognized network controller %X:%X\n", 
                       PCIGetVendorId(busNumber, deviceNumber, functionNumber),
                       PCIGetDeviceId(busNumber, deviceNumber, functionNumber));
            }
            break;
        default:
            break;
    }
}

void PciBridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass)
{
    switch (Subclass)
    {
        case PCI_SUBCLASS_PCI_TO_PCI_BRIDGE:;
            PciToPciGetSecondaryBus(busNumber, deviceNumber, functionNumber);
            break;
        default:
            break;
    }
}

PCI_IDE_CONTROLLER Controllers[MAX_PCI_IDE_CONTROLLERS] = { 0 };

BOOL AlreadyContainsController(PCI_IDE_CONTROLLER* controller)
{
    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
    {
        if (Controllers[i].Channels[0].Base == controller->Channels[0].Base &&
            Controllers[i].Channels[0].Ctrl == controller->Channels[0].Ctrl &&
            Controllers[i].Channels[1].Base == controller->Channels[1].Base &&
            Controllers[i].Channels[1].Ctrl == controller->Channels[1].Ctrl)
        {
            return TRUE;
        }
    }
    return FALSE;
}

UINT64 NumberOfControllers()
{
    UINT64 result = 0;

    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
    {
        result += ((Controllers[i].Channels[0].Base) != 0);
    }

    return result;
}

int AddController(PCI_IDE_CONTROLLER p_ide_ctrl)
{
    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
    {
        if (!(Controllers[i].Channels[0].Base))
        {
            Controllers[i] = p_ide_ctrl;
            return i;
        }
    }
    return -1;
}

void PciMassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass)
{
    BYTE progIF = PciGetProgIf(busNumber, deviceNumber, functionNumber);

    if (Subclass == PCI_SUBCLASS_IDE_CONTROLLER)
    {
        if (NumberOfControllers() < MAX_PCI_IDE_CONTROLLERS - 1)
        {
            PCI_IDE_CONTROLLER controller = PciIdeInitPciDevice(busNumber, deviceNumber, functionNumber, progIF);
            if (!AlreadyContainsController(&controller))
            {
                int id;
                UINT8 commandByte;

                id = AddController(controller);

                commandByte = PciConfigReadWord(busNumber, deviceNumber, functionNumber, 4) & 0xFF;
                commandByte |= 2;

                PciConfigWriteByte(busNumber, deviceNumber, functionNumber, 4, commandByte);
                SearchForDevices(Controllers + id);
            }
        }
        else
        {
            PrintT("Too many controllers!\n");
        }
    }

}

IDE_DRIVE Drives[MAX_PCI_IDE_CONTROLLERS * 4] = { 0 };

int AddDrive(IDE_DRIVE* drive)
{
    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++)
    {
        if (Drives[i].Reserved == 0)
        {
            Drives[i] = *drive;
            return i;
        }
    }
    return -1;
}

void PciIdeEnumerate()
{
    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++)
    {
        Drives[i].Reserved = 0;
    }

    for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
    {
        if (Controllers[i].Channels[0].Base)
        {
            PCI_IDE_CONTROLLER* c = Controllers + i;
            for (int j = 0; j < 4; j++)
            {
                if (c->Drives[j].Reserved == 1)
                {
                    c->Drives[j].controller = Controllers + i;
                    AddDrive(c->Drives + j);
                }
            }
        }
    }

    DiskCheck();
}
