#ifndef NNX_PCI_HEADER
#define NNX_PCI_HEADER
#include "nnxint.h"
#include "../../video/SimpleTextIO.h"
#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define PCI_CLASS_UNCLASSIFIED 0
	#define PCI_SUBCLASS_NON_VGA_COMPATIBLE 0
	#define PCI_SUBCLASS_VGA_COMPATIBLE 1
#define PCI_CLASS_MASS_STORAGE_CONTROLLER 0X1
	#define	PCI_SUBCLASS_SCSI_BUS_CONTROLLER 0x0
	#define PCI_SUBCLASS_IDE_CONTROLLER 0x1
	#define PCI_SUBCLASS_FLOPPY_DISK_CONTROLLER 0x2
	#define PCI_SUBCLASS_IPI_BUS_CONTROLLER 0X3
	#define PCI_SUBCLASS_ATA_CONTROLLER 0X4
	#define PCI_SUBCLASS_SERIAL_ATA_CONTROLLER 0X5
	#define PCI_SUBCLASS_SERIAL_SCSI_CONTROLLER 0X6
	#define PCI_SUBCLASS_NON_VOLATILE_MEMORY_CONTROLLER 0X7
#define PCI_CLASS_NETWORK_CONTROLLER 0X2
#define PCI_CLASS_DISPLAY_CONTROLLER 0X3
#define PCI_CLASS_MULTIMEDIA_CONTROLLER 0X4
#define PCI_CLASS_MEMORY_CONTROLLER 0X5
#define PCI_CLASS_BRIDGE_DEVICE 0X6
	#define PCI_SUBCLASS_HOST_BRIDGE 0x0
	#define PCI_SUBCLASS_ISA_BRIDGE 0x1
	#define PCI_SUBCLASS_EISA_BRIDGE 0x2
	#define PCI_SUBCLASS_MCA_BRIDGE 0X3
	#define PCI_SUBCLASS_PCI_TO_PCI_BRIDGE 0X4
	#define PCI_SUBCLASS_PCMCIA_BRIDGE 0x5
	#define PCI_SUBCLASS_NuBus_BRIDGE 0x6
	#define PCI_SUBCLASS_CardBus_BRIDGE 0x7
	#define PCI_SUBCLASS_RACEway_BRIDGE 0X8
	#define PCI_SUBCLASS_PCI_TO_PCI_SEMITRANSPARENT_BRIDGE 0x9
	#define PCI_SUBCLASS_InfiniBand_TO_PCI_HOST_BRIDGE 0xA
#define PCI_CLASS_SIMPLE_COMMUNICATION_CONTROLLER 0x7
#define PCI_CLASS_BASE_SYSTEM_PERIPHERAL 0x8
#define PCI_CLASS_INPUT_DEVICE_CONTROLLER 0x9
#define PCI_CLASS_DOCKING_STATION 0XA
#define PCI_CLASS_PROCESSOR 0XB
#define PCI_CLASS_SERIAL_BUS_CONTROLLER 0xC
#define PCI_CLASS_WIRELESS_CONTROLLER 0XD
#define PCI_CLASS_UNSPECIFIED 0XFF
#define PCI_SUBCLASS_OTHER 0x80


UINT16 PCIConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset);
UINT32 PCIConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset);

inline UINT16 PCIGetVendor(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0);
}

inline bool PCICheckIfPresent(UINT8 bus, UINT8 slot, UINT8 function) {

	return PCIGetVendor(bus, slot, function) != 0xffff;
}

inline UINT8 PCIGetHeader(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0xe) & 0xff;
}

inline UINT8 PCIGetSubclass(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0xa) & 0xff;
}

inline UINT8 PCIGetClass(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0xb) & 0xff;
}

inline UINT8 PCItoPCIGetSecondaryBus(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0x19) & 0xff;
}

inline UINT8 PCIGetProgIF(UINT8 bus, UINT8 slot, UINT8 function) {
	return PCIConfigReadWord(bus, slot, function, 0x9) & 0xff;
}

inline UINT32 PCIGetBAR(UINT8 bus, UINT8 slot, UINT8 function, UINT32 n) {
	return PCIConfigReadDWord(bus, slot, function, (UINT8)(0x10 + 0x4 * n));
}

#define PCIGetBAR0(b,s,f) PCIGetBAR(b,s,f,0)
#define PCIGetBAR1(b,s,f) PCIGetBAR(b,s,f,1)
#define PCIGetBAR2(b,s,f) PCIGetBAR(b,s,f,2)
#define PCIGetBAR3(b,s,f) PCIGetBAR(b,s,f,3)
#define PCIGetBAR4(b,s,f) PCIGetBAR(b,s,f,4)

UINT8 PCIScan();
void PCIScanBus(UINT8 bus);
void PCIScanDevice(UINT8 bus, UINT8 device);
void PCIScanFunction(UINT8 bus, UINT8 device, UINT8 function);

#endif