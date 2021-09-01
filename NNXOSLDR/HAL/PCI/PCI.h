#ifndef NNX_PCI_HEADER
#define NNX_PCI_HEADER
#include "nnxint.h"
#include "../../video/SimpleTextIo.h"

#ifdef __cplusplus
extern "C"{
#endif

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


UINT16 PciConfigReadWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset);
UINT32 PciConfigReadDWord(UINT8 bus, UINT8 slot, UINT8 function, UINT8 offset);

inline UINT16 PCIGetVendor(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0);
}

inline bool PciCheckIfPresent(UINT8 bus, UINT8 slot, UINT8 function) {

	return PCIGetVendor(bus, slot, function) != 0xffff;
}

inline UINT8 PciGetHeader(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0xe) & 0xff;
}

inline UINT8 PciGetSubclass(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0xa) & 0xff;
}

inline UINT8 PCIGetClass(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0xb) & 0xff;
}

inline UINT8 PciToPciGetSecondaryBus(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0x19) & 0xff;
}

inline UINT8 PciGetProgIf(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciConfigReadWord(bus, slot, function, 0x9) & 0xff;
}

inline UINT32 PciGetBar(UINT8 bus, UINT8 slot, UINT8 function, UINT32 n) {
	return PciConfigReadDWord(bus, slot, function, (UINT8)(0x10 + 0x4 * n));
}

inline UINT32 PciGetBar0(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciGetBar(bus, slot, function, 0);
}

inline UINT32 PciGetBar1(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciGetBar(bus, slot, function, 1);
}

inline UINT32 PciGetBar2(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciGetBar(bus, slot, function, 2);
}

inline UINT32 PciGetBar3(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciGetBar(bus, slot, function, 3);
}

inline UINT32 PciGetBar4(UINT8 bus, UINT8 slot, UINT8 function) {
	return PciGetBar(bus, slot, function, 4);
}

UINT8 PciScan();
void PciScanBus(UINT8 bus);
void PciScanDevice(UINT8 bus, UINT8 device);
void PciScanFunction(UINT8 bus, UINT8 device, UINT8 function);

#ifdef __cplusplus
}
#endif

#endif