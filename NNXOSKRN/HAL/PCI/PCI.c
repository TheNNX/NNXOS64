#include "PCI.h"
#include <SimpleTextIo.h>
#include <HAL/Port.h>
#include "device/fs/mbr.h"
#include "device/fs/gpt.h"

#pragma warning(disable : 4189)

void PciBridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
void PciMassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
VOID PciIdeEnumerate();

UINT16 PciConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset)
{
	UINT32 address = (UINT32) ((((UINT32) bus) << 16) | (((UINT32) slot) << 11) | (((UINT32) function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return (UINT16) ((ind(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

UINT32 PciConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset)
{
	UINT32 address = (UINT32) ((((UINT32) bus) << 16) | (((UINT32) slot) << 11) | (((UINT32) function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return ind(CONFIG_DATA) >> ((offset & 2) * 8);
}

VOID PciConfigWriteByte(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset, UINT8 byte)
{
	UINT32 address = (UINT32) ((((UINT32) bus) << 16) | (((UINT32) slot) << 11) | (((UINT32) function) << 8) | (offset & 0xfc) | 0x80000000);
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
		return;
	for (UINT8 device = 0;; device++)
	{
		PciScanDevice(busNumber, device);
		if (device == 0xff)
			break;
	}
}

void PciScanDevice(UINT8 busNumber, UINT8 deviceNumber)
{
	if (!PciCheckIfPresent(busNumber, deviceNumber, 0))
		return;
	PciScanFunction(busNumber, deviceNumber, 0);
	UINT8 header = PciGetHeader(busNumber, deviceNumber, 0);
	if (header & 0x80)
	{
		for (UINT8 function = 1; function < 0x7f; function++)
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
	switch (Class)
	{
		case PCI_CLASS_BRIDGE_DEVICE:
			PciBridgeDeviceClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
			break;
		case PCI_CLASS_MASS_STORAGE_CONTROLLER:
			PciMassStorageClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
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
			UINT8 secondaryBus = PciToPciGetSecondaryBus(busNumber, deviceNumber, functionNumber);

			break;
		default:
			break;
	}
}

#include "HAL/PCI/PCIIDE.h"

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
			return true;
		}
	}
	return false;
}

UINT64 NumberOfControllers()
{
	UINT64 result = 0;

	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
		result += ((Controllers[i].Channels[0].Base) != 0);

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
			PCI_IDE_CONTROLLER controller = PciIdeInitPciDevice(busNumber, deviceNumber, functionNumber, PciGetProgIf(busNumber, deviceNumber, functionNumber));
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

//some debug functionality
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
