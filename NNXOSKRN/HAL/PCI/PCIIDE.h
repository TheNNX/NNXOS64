#ifndef NNX_PCIIDE_HEADER
#define NNX_PCIIDE_HEADER
#include "HAL/PCI/PCI.h"
#include "../../device/fs/vfs.h"

//All consts from https://wiki.osdev.org/PCI_IDE_Controller

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct _PCI_IDE_CONTROLLER PCI_IDE_CONTROLLER;

#include "device/hdd/hdd.h"

	typedef struct _IDE_DRIVE
	{
		UINT8 Reserved;
		UINT8 channel;
		UINT8 drive;
		UINT16 type;
		UINT16 signature;
		UINT16 capabilities;
		UINT32 commandSets;
		UINT64 size;
		unsigned char model[41];
		PCI_IDE_CONTROLLER* controller;
		CHS geometry;
	} IDE_DRIVE;


#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define      ATAPI_CMD_READ       0xA8
#define      ATAPI_CMD_EJECT      0x1B

#define MAX_PCI_IDE_CONTROLLERS 32
#define IDE_LBA_SUPPORT 0x200

	struct _PCI_IDE_CONTROLLER
	{
		UINT8 FunctionNumber;
		UINT8 DeviceNumber;
		UINT8 BusNumber;
		UINT8 ProgIf;

		UINT32 Bar0;
		UINT32 Bar1;
		UINT32 Bar2;
		UINT32 Bar3;
		UINT32 Bar4;

		struct IDE_CHANNELS
		{
			unsigned short Base;
			unsigned short Ctrl;
			unsigned short BusMasterIde;
			BOOL NoInterrupt;
		} Channels[2];

		unsigned char IdeIrqInvoked;
		unsigned char AtapiPacket[12];
		unsigned char IdeBuffer[2048];

		unsigned char InterruptNumber;

		IDE_DRIVE Drives[4];
	};

	extern IDE_DRIVE Drives[MAX_PCI_IDE_CONTROLLERS * 4];
	extern PCI_IDE_CONTROLLER Controllers[MAX_PCI_IDE_CONTROLLERS];
	
	VOID SearchForDevices(PCI_IDE_CONTROLLER* pic);
	PCI_IDE_CONTROLLER PciIdeInitPciDevice(UINT8 bus, UINT8 device, UINT8 function, UINT8 progIf);
	VFS_STATUS PciIdeDiskIo(IDE_DRIVE* drive, UINT8 direction, UINT64 lba, UINT16 numberOfSectors, UINT8* dest);

#ifdef __cplusplus
}
#endif

#endif
