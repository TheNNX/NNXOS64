#ifndef NNX_PCIIDE_HEADER
#define NNX_PCIIDE_HEADER
#include "HAL/PCI/PCI.h"

//All consts from https://wiki.osdev.org/PCI_IDE_Controller

typedef struct PCI_IDE_Controller PCI_IDE_Controller;

typedef struct CHS {
	UINT32 CHS : 24;
	struct
	{
		UINT32 sector : 6;
		UINT32 head : 4;
		UINT32 cylinder : 14;
	};
}CHS;

typedef struct IDEDrive {
	UINT8 reserved;
	UINT8 channel;
	UINT8 drive;
	UINT16 type;
	UINT16 signature;
	UINT16 capabilities;
	UINT32 commandSets;
	UINT64 size;
	unsigned char model[41];
	PCI_IDE_Controller* controller;
	CHS geometry;

} IDEDrive;

UINT64 PCI_IDE_DiskIO(IDEDrive* drive, UINT8 direction, UINT64 lba, UINT16 numberOfSectors, UINT8* dest);

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

struct PCI_IDE_Controller {
	UINT8 functionNumber;
	UINT8 deviceNumber;
	UINT8 busNumber;
	UINT8 progIf;

	UINT32 BAR0;
	UINT32 BAR1;
	UINT32 BAR2;
	UINT32 BAR3;
	UINT32 BAR4;

	struct IDE_Channels {
		unsigned short base;
		unsigned short ctrl;
		unsigned short bus_master_ide;
		bool no_interrupt;
	} channels[2];

	unsigned char ide_irq_invoked;
	unsigned char atapi_packet[12];
	unsigned char ide_buffer[2048];

	unsigned char interrupt_number;

	IDEDrive drives[4];
};

#define IDE_LBA_SUPPORT 0x200

PCI_IDE_Controller PCIIDE_InitPCIDevice(UINT8 bus, UINT8 device, UINT8 function, UINT8 progIf);
#endif