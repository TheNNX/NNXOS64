#include "PCI.h"
#include "video/SimpleTextIO.h"
#include "HAL/Port.h"

UINT16 PCIConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset){
	UINT32 address = (UINT32)((((UINT32)bus)<<16) | (((UINT32)slot)<<11) | (((UINT32)function)<<8) | (offset & 0xfc) | 0x80000000);
	outd(CONFIG_ADDRESS, address);
	return (UINT16)((ind(CONFIG_DATA) >> ((offset & 2) * 8)) & 0xffff);
}

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

void PCI_BridgeDeviceClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 subclass);
void PCI_MassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass);

void PCIScanFunction(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber) {
	//PrintT("Bus: %x Device: %x Function: %x\n", busNumber, deviceNumber, functionNumber);
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
	PrintT("PCI: %x %x %x %x %x\n", busNumber, deviceNumber, functionNumber, Class, Subclass);
	switch (Subclass)
	{
	case PCI_SUBCLASS_PCI_TO_PCI_BRIDGE:;
		UINT8 secondaryBus = PCItoPCIGetSecondaryBus(busNumber, deviceNumber, functionNumber);

		break;
	default:
		break;
	}
}

void PCI_MassStorageClass(UINT8 busNumber, UINT8 deviceNumber, UINT8 functionNumber, UINT8 Class, UINT8 Subclass) {
	PrintT("mass storage: %x %x %x %x %x\n", busNumber, deviceNumber, functionNumber, Class, Subclass);
}