#include "PCI.h"
#include "video/SimpleTextIo.h"
#include "HAL/Port.h"
#include "device/fs/mbr.h"
#include "device/fs/gpt.h"

#pragma warning(disable : 4189)

UINT16 PciConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset){
	UINT32 address = (UINT32)((((UINT32)bus)<<16) | (((UINT32)slot)<<11) | (((UINT32)function)<<8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return (UINT16)((ind(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

UINT32 PciConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset) {
	UINT32 address = (UINT32)((((UINT32)bus) << 16) | (((UINT32)slot) << 11) | (((UINT32)function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return ind(CONFIG_DATA) >> ((offset & 2) * 8);
}

VOID PciConfigWriteByte(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset, UINT8 byte) {
	UINT32 address = (UINT32)((((UINT32)bus) << 16) | (((UINT32)slot) << 11) | (((UINT32)function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	outb(CONFIG_DATA, byte);
}

VOID PciIdeEnumerate();

UINT8 PciScan() {
	UINT8 header = PciGetHeader(0, 0, 0);
	if (header & 0x80) {
		PrintT("One PCI bus\n");
		PciScanBus(0);
	}
	else {
		PrintT("Multiple PCI buses\n");
		for (UINT8 function = 0;; function++) {
			UINT8 bus = function;
			PciScanBus(bus);
			if (function == 0xff)
				break;
		}
	}

	PciIdeEnumerate();
}

void PciScanBus(UINT8 busNumber) {
	if (!PciCheckIfPresent(0, 0, busNumber))
		return;
	for (UINT8 device = 0;; device++) {
		PciScanDevice(busNumber, device);
		if (device == 0xff)
			break;
	}
}

void PciScanDevice(UINT8 busNumber, UINT8 deviceNumber) {
	if (!PciCheckIfPresent(busNumber, deviceNumber, 0))
		return;
	PciScanFunction(busNumber, deviceNumber, 0);
	UINT8 header = PciGetHeader(busNumber, deviceNumber, 0);
	if (header & 0x80) {
		for (UINT8 function = 1; function < 0x7f; function++) {
			PciScanFunction(busNumber, deviceNumber, function);
		}
	}
}

void PciBridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
void PciMassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);

void PciScanFunction(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber) {
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

void PciBridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass){
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

PCI_IDE_CONTROLLER controllers[MAX_PCI_IDE_CONTROLLERS] = { 0 };

bool AlreadyContainsController(PCI_IDE_CONTROLLER* controller) { //pointer used, so only the address is passed on the stack (well, in this case onto RCX), and not the entire structure
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++) {
		if (controllers[i].channels[0].base == controller->channels[0].base &&
			controllers[i].channels[0].ctrl == controller->channels[0].ctrl &&
			controllers[i].channels[1].base == controller->channels[1].base &&
			controllers[i].channels[1].ctrl == controller->channels[1].ctrl) {
			return true;
		}
	}
	return false;
}

UINT64 NumberOfControllers() {
	UINT64 result = 0;

	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
		result += ((controllers[i].channels[0].base) != 0);

	return result;
}

int AddController(PCI_IDE_CONTROLLER p_ide_ctrl) {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++) {
		if (!(controllers[i].channels[0].base)) {
			controllers[i] = p_ide_ctrl;
			return i;
		}
	}
	return -1;
}

void PciMassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass) {
	BYTE progIF = PciGetProgIf(busNumber, deviceNumber, functionNumber);

	if (Subclass == PCI_SUBCLASS_IDE_CONTROLLER) {
		if (NumberOfControllers() < MAX_PCI_IDE_CONTROLLERS - 1) {
			PCI_IDE_CONTROLLER controller = PciIdeInitPciDevice(busNumber, deviceNumber, functionNumber, PciGetProgIf(busNumber, deviceNumber, functionNumber));
			if (!AlreadyContainsController(&controller)) {
				int ID = AddController(controller);
				PrintT("Added controller %d\n", ID);
				UINT8 commandByte = PciConfigReadWord(busNumber, deviceNumber, functionNumber, 4) & 0xFF;
				PrintT("Command word: 0b%b\n",commandByte);
				commandByte |= 2;
				PciConfigWriteByte(busNumber, deviceNumber, functionNumber, 4, commandByte);
				SearchForDevices(controllers+ID);
			}
			else {
				PrintT("Controller already added.\n");
			}
		}
		else {
			PrintT("Too many controllers!\n");
		}
	}
	
}

IDE_DRIVE drives[MAX_PCI_IDE_CONTROLLERS * 4] = { 0 };

int AddDrive(IDE_DRIVE* drive) {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		if (drives[i].Reserved == 0) {
			drives[i] = *drive;
			return i;
		}
	}
	return -1;
}


//some debug functionality
void PciIdeEnumerate() {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		drives[i].Reserved = 0;
	}

	PrintT("Enumerating PCI IDE devices.\n");
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++) {
		if (controllers[i].channels[0].base) {
			PCI_IDE_CONTROLLER* c = controllers + i;
			for (int j = 0; j < 4; j++) {
				if (c->drives[j].Reserved == 1) {
					c->drives[j].controller = controllers+i;
					AddDrive(c->drives + j);
				}
			}
		}
	}

	DiskCheck();
	
}
