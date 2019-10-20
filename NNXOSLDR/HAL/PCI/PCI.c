#include "PCI.h"
#include "video/SimpleTextIO.h"
#include "HAL/Port.h"

UINT16 PCIConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset){
	UINT32 address = (UINT32)((((UINT32)bus)<<16) | (((UINT32)slot)<<11) | (((UINT32)function)<<8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return (UINT16)((ind(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

UINT32 PCIConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset) {
	UINT32 address = (UINT32)((((UINT32)bus) << 16) | (((UINT32)slot) << 11) | (((UINT32)function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return ind(CONFIG_DATA) >> ((offset & 2) * 8);
}

VOID PCIConfigWriteByte(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset, UINT8 byte) {
	UINT32 address = (UINT32)((((UINT32)bus) << 16) | (((UINT32)slot) << 11) | (((UINT32)function) << 8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	outb(CONFIG_DATA, byte);
}

VOID PCI_IDE_Enumerate();

UINT8 PCIScan() {
	UINT8 header = PCIGetHeader(0, 0, 0);
	if (header & 0x80) {
		PrintT("One PCI bus\n");
		PCIScanBus(0);
	}
	else {
		PrintT("Multiple PCI buses\n");
		for (UINT8 function = 0;; function++) {
			UINT8 bus = function;
			PCIScanBus(bus);
			if (function == 0xff)
				break;
		}
	}

	PCI_IDE_Enumerate();
}

void PCIScanBus(UINT8 busNumber) {
	if (!PCICheckIfPresent(0, 0, busNumber))
		return;
	for (UINT8 device = 0;; device++) {
		PCIScanDevice(busNumber, device);
		if (device == 0xff)
			break;
	}
}

void PCIScanDevice(UINT8 busNumber, UINT8 deviceNumber) {
	if (!PCICheckIfPresent(busNumber, deviceNumber, 0))
		return;
	PCIScanFunction(busNumber, deviceNumber, 0);
	UINT8 header = PCIGetHeader(busNumber, deviceNumber, 0);
	if (header & 0x80) {
		for (UINT8 function = 1; function < 0x7f; function++) {
			PCIScanFunction(busNumber, deviceNumber, function);
		}
	}
}

void PCI_BridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);
void PCI_MassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);

void PCIScanFunction(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber) {
	if (!PCICheckIfPresent(busNumber, deviceNumber, functionNumber))
		return;
	UINT8 Class = PCIGetClass(busNumber, deviceNumber, functionNumber);
	UINT8 Subclass = PCIGetSubclass(busNumber, deviceNumber, functionNumber);
	switch (Class)
	{
	case PCI_CLASS_BRIDGE_DEVICE:
		PCI_BridgeDeviceClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
		break;
	case PCI_CLASS_MASS_STORAGE_CONTROLLER:
		PCI_MassStorageClass(busNumber, deviceNumber, functionNumber, Class, Subclass);
		break;
	default:
		break;
	}
}

void PCI_BridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass){
	switch (Subclass)
	{
	case PCI_SUBCLASS_PCI_TO_PCI_BRIDGE:;
		UINT8 secondaryBus = PCItoPCIGetSecondaryBus(busNumber, deviceNumber, functionNumber);

		break;
	default:
		break;
	}
}

#include "HAL/PCI/PCIIDE.h"

#define MAX_PCI_IDE_CONTROLLERS 32

PCI_IDE_Controller controllers[MAX_PCI_IDE_CONTROLLERS] = { 0 };

bool alreadyContainsController(PCI_IDE_Controller* controller) { //pointer used, so only the address is passed on the stack (well, in this case onto RCX), and not the entire structure
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

UINT64 numberOfControllers() {
	UINT64 result = 0;

	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++)
		result += ((controllers[i].channels[0].base) != 0);

	return result;
}

int addController(PCI_IDE_Controller p_ide_ctrl) {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++) {
		if (!(controllers[i].channels[0].base)) {
			controllers[i] = p_ide_ctrl;
			return i;
		}
	}
	return -1;
}

void PCI_MassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass) {
	BYTE progIF = PCIGetProgIF(busNumber, deviceNumber, functionNumber);

	if (Subclass == PCI_SUBCLASS_IDE_CONTROLLER) {
		if (numberOfControllers() < MAX_PCI_IDE_CONTROLLERS - 1) {
			PCI_IDE_Controller controller = PCIIDE_InitPCIDevice(busNumber, deviceNumber, functionNumber, PCIGetProgIF(busNumber, deviceNumber, functionNumber));
			if (!alreadyContainsController(&controller)) {
				int ID = addController(controller);
				PrintT("Added controller %d\n", ID);
				UINT8 commandByte = PCIConfigReadWord(busNumber, deviceNumber, functionNumber, 4) & 0xFF;
				PrintT("Command word: 0b%b\n",commandByte);
				commandByte |= 2;
				PCIConfigWriteByte(busNumber, deviceNumber, functionNumber, 4, commandByte);
				SearchForDevices(controllers+ID);
			}
			else {
				PrintT("Controller already added.\n");
			}
		}
		else {
			PrintT("To many controllers!\n");
		}
	}
	
}

IDEDrive drives[MAX_PCI_IDE_CONTROLLERS * 4] = { 0 };

int AddDrive(IDEDrive* drive) {
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		if (drives[i].reserved == 0) {
			drives[i] = *drive;
			return i;
		}
	}
	return -1;
}


//some debug functionality
void PCI_IDE_Enumerate() {
	UINT8 buffer[4096] = {0};
	UINT8 *s = "uhduwhudhwjhdjwhjdwhjd";

	UINT8 *sO = s;
	while (*s) buffer[(s - sO)] = *(s++);

	PrintT("Enumerating PCI IDE devices.\n");
	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS; i++) {
		if (controllers[i].channels[0].base) {
			PCI_IDE_Controller* c = controllers + i;
			for (int j = 0; j < 4; j++) {
				if (c->drives[j].reserved == 1) {
					c->drives[j].controller = controllers+i;
					AddDrive(c->drives + j);
				}
			}
		}
	}

	for (int i = 0; i < MAX_PCI_IDE_CONTROLLERS * 4; i++) {
		if (drives[i].reserved) {
			PrintT("   %x   -  %s Drive [%iMiB]\n", drives[i].signature, (const char*[]){"ATA", "ATAPI"}[drives[i].type], drives[i].size/1024/1024);
		}
	}
	PrintT("Starting disk read to location 0x%x.\n", buffer);
	
	PCI_IDE_DiskIO(drives + 1, 1, 0, 1, buffer);
	UINT8 buffer2[4096];
	UINT64 a = PCI_IDE_DiskIO(drives + 1, 0, 0, 1, buffer2);

	for (int i = 0; i < 512; i++) {
		if (buffer2[i] >= '!')
			PrintT("%c", buffer2[i]);
		else
			PrintT(".");
	}
	
}