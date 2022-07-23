#include "HAL/PCI/PCIIDE.h"
#include <SimpleTextIo.h>
#include "HAL/Port.h"
#include "device/hdd/hdd.h"

#define WAIT_MACRO for (int __ = 0; __ < 10000; __++);

VOID DiskReadLong(UINT16 port, UCHAR * buffer, UINT32 count);

VOID IdeWrite(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg, UINT8 data)
{
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, 0x80 | pic->Channels[channel].NoInterrupt);
	if (reg < 0x08)
		outb(pic->Channels[channel].Base + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(pic->Channels[channel].Base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(pic->Channels[channel].Ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(pic->Channels[channel].BusMasterIde + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, pic->Channels[channel].NoInterrupt);
}

UINT8 IdeRead(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg)
{
	unsigned char result = 0;
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, 0x80 | pic->Channels[channel].NoInterrupt);
	if (reg < 0x08)
		result = inb(pic->Channels[channel].Base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(pic->Channels[channel].Base + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(pic->Channels[channel].Ctrl + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(pic->Channels[channel].BusMasterIde + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, pic->Channels[channel].NoInterrupt);
	return result;
}

VOID FillBars(PCI_IDE_CONTROLLER* pic)
{
	pic->Bar0 = PciGetBar0(pic->BusNumber, pic->DeviceNumber, pic->FunctionNumber);
	pic->Bar1 = PciGetBar1(pic->BusNumber, pic->DeviceNumber, pic->FunctionNumber);
	pic->Bar2 = PciGetBar2(pic->BusNumber, pic->DeviceNumber, pic->FunctionNumber);
	pic->Bar3 = PciGetBar3(pic->BusNumber, pic->DeviceNumber, pic->FunctionNumber);
	pic->Bar4 = PciGetBar4(pic->BusNumber, pic->DeviceNumber, pic->FunctionNumber);
	pic->Channels[0].Base = (pic->Bar0 & 0xFFFFFFFC) + 0x1F0 * (!pic->Bar0);
	pic->Channels[0].Ctrl = (pic->Bar1 & 0xFFFFFFFC) + 0x3F6 * (!pic->Bar1);
	pic->Channels[1].Base = (pic->Bar2 & 0xFFFFFFFC) + 0x170 * (!pic->Bar2);
	pic->Channels[1].Ctrl = (pic->Bar3 & 0xFFFFFFFC) + 0x376 * (!pic->Bar3);
	pic->Channels[0].BusMasterIde = (pic->Bar4 & 0xFFFFFFFC);
	pic->Channels[1].BusMasterIde = (pic->Bar4 & 0xFFFFFFFC) + 8;
}


VOID IdeReadBuffer(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg, UCHAR* buffer, UINT32 quads)
{

	if (reg < 0x08)
		DiskReadLong(pic->Channels[channel].Base + reg - 0x00, buffer, quads);
	else if (reg < 0x0C)
		DiskReadLong(pic->Channels[channel].Base + reg - 0x06, buffer, quads);
	else if (reg < 0x0E)
		DiskReadLong(pic->Channels[channel].Ctrl + reg - 0x0A, buffer, quads);
	else if (reg < 0x16)
		DiskReadLong(pic->Channels[channel].BusMasterIde + reg - 0x0E, buffer, quads);

}

VOID SearchForDevices(PCI_IDE_CONTROLLER* pic)
{
	int diskCount = 0;

	IdeWrite(pic, 0, ATA_REG_CONTROL, 2);
	IdeWrite(pic, 1, ATA_REG_CONTROL, 2);

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			UINT8 status;
			UINT8 type;
			UINT8 error;

			pic->Drives[diskCount].Reserved = 0;
			IdeWrite(pic, i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
			WAIT_MACRO;

			IdeWrite(pic, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			WAIT_MACRO;

			type = 0;
			error = 0;
			status = IdeRead(pic, i, ATA_REG_STATUS);
			
			if (status == 0)
				continue;

			while (1)
			{
				status = IdeRead(pic, i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR))
				{
					error = 1; 
					break;
				}
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			if (error != 0)
			{
				unsigned char lba1 = IdeRead(pic, i, ATA_REG_LBA1);
				unsigned char lba2 = IdeRead(pic, i, ATA_REG_LBA2);

				if (lba1 == 0x14 && lba2 == 0xEB)
					type = 1;
				else if (lba1 == 0x69 && lba2 == 0x96)
					type = 1;
				else
					continue;

				IdeWrite(pic, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				status = IdeRead(pic, i, ATA_REG_STATUS);
				error = status & ATA_SR_ERR;
				WAIT_MACRO;
			}

			IdeReadBuffer(pic, i, ATA_REG_DATA, pic->IdeBuffer, 128);

			pic->Drives[diskCount].Reserved = 1;
			pic->Drives[diskCount].type = type;
			pic->Drives[diskCount].channel = i;
			pic->Drives[diskCount].drive = j;
			pic->Drives[diskCount].signature = *((UINT16*) (pic->IdeBuffer + ATA_IDENT_DEVICETYPE));
			pic->Drives[diskCount].capabilities = *((UINT16*) (pic->IdeBuffer + ATA_IDENT_CAPABILITIES));
			pic->Drives[diskCount].commandSets = *((UINT32*) (pic->IdeBuffer + ATA_IDENT_COMMANDSETS));

			pic->Drives[diskCount].geometry.sector = *((UINT8*) (pic->IdeBuffer + ATA_IDENT_SECTORS));
			pic->Drives[diskCount].geometry.head = *((UINT8*) (pic->IdeBuffer + ATA_IDENT_HEADS));
			pic->Drives[diskCount].geometry.cylinder = *((UINT16*) (pic->IdeBuffer + ATA_IDENT_CYLINDERS));

			if (pic->Drives[diskCount].commandSets & (1 << 26))
				pic->Drives[diskCount].size = *((unsigned int *) (pic->IdeBuffer + ATA_IDENT_MAX_LBA_EXT));
			else
				pic->Drives[diskCount].size = *((unsigned int *) (pic->IdeBuffer + ATA_IDENT_MAX_LBA));

			pic->Drives[diskCount].size *= 512;

			for (int k = 0; k < 40; k += 2)
			{
				pic->Drives[diskCount].model[k] = pic->IdeBuffer[ATA_IDENT_MODEL + k + 1];
				pic->Drives[diskCount].model[k + 1] = pic->IdeBuffer[ATA_IDENT_MODEL + k];
			}
			pic->Drives[diskCount].model[40] = 0;
			pic->Drives[diskCount].controller = 0;  
			diskCount++;
		}
	}

}

PCI_IDE_CONTROLLER PciIdeInitPciDevice(UINT8 bus, UINT8 device, UINT8 function, UINT8 progIf)
{
	PCI_IDE_CONTROLLER pic = { 0 };
	pic.BusNumber = bus;
	pic.DeviceNumber = device;
	pic.FunctionNumber = function;
	pic.ProgIf = progIf;

	pic.InterruptNumber = PciConfigReadWord(bus, device, function, 0x3c) & 0xff;
	PciConfigWriteByte(bus, device, function, 0x3f, 0xfe);
	pic.InterruptNumber = PciConfigReadWord(bus, device, function, 0x3c) & 0xff;

	if (pic.InterruptNumber == 0xFE)
	{
		PciConfigWriteByte(bus, device, function, 0x3f, 14);
		pic.InterruptNumber = 14;
	}
	else
	{
		if (progIf == 0x80 || progIf == 0x8A)
		{ 
			// parallel IDE
			pic.InterruptNumber = 210;
		}
	}

	FillBars(&pic);
	return pic;
}

UINT8 IdePoll(PCI_IDE_CONTROLLER* controller, UINT8 channel, BOOL setError)
{
	for (int i = 0; i < 4; i++)
	{
		IdeRead(controller, channel, ATA_REG_ALTSTATUS);
	}

	WAIT_MACRO;

	UINT8 status;
	while (status = IdeRead(controller, channel, ATA_REG_STATUS))
	{
		if (!(status & ATA_SR_BSY))
			break;
	}

	if (setError)
	{
		if (status & ATA_SR_ERR)
		{
			PrintTA("ERR\n");
			return status;
		}
		if (status & ATA_SR_DF)
		{
			PrintTA("DF\n");
			return status;
		}
		if (!(status & ATA_SR_DRQ))
		{
			PrintTA("DRQ\n");
			return status;
		}
	}
	return 0;
}

VFS_STATUS PciIdeDiskIo(IDE_DRIVE* drive, UINT8 direction, UINT64 lba, UINT16 numberOfSectors, UINT8* buffer)
{
	UINT64 i, j;
	UINT8 channel = drive->channel;
	BOOL isSlave = drive->drive;
	PCI_IDE_CONTROLLER* controller = drive->controller;
	UINT16 bus = controller->Channels[channel].Base;

	controller->Channels[channel].NoInterrupt = 1;
	IdeWrite(controller, channel, ATA_REG_CONTROL, 0x02);

	while (IdeRead(controller, channel, ATA_REG_STATUS) & ATA_SR_BSY);

	if (drive->capabilities & IDE_LBA_SUPPORT)
	{
		IdeWrite(controller, channel, ATA_REG_HDDEVSEL, 0xE0 | (isSlave ? 16 : 0) | ((lba > 0xFFFFFFF) ? 0 : ((lba & 0xF000000) >> 24))); // Drive & LBA
		if (lba > 0xFFFFFFF)
		{

			IdeWrite(controller, channel, ATA_REG_SECCOUNT1, (UCHAR)((numberOfSectors & 0xFF00) >> 8));
			IdeWrite(controller, channel, ATA_REG_LBA3, (lba >> 24) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA4, (lba >> 32) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA5, (lba >> 40) & 0x0F);

			IdeWrite(controller, channel, ATA_REG_SECCOUNT0, (UCHAR)numberOfSectors);
			IdeWrite(controller, channel, ATA_REG_LBA0, lba & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA1, (lba >> 8) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA2, (lba >> 16) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO_EXT : ATA_CMD_READ_PIO_EXT);
		}
		else
		{
			IdeWrite(controller, channel, ATA_REG_SECCOUNT0, (UCHAR)numberOfSectors);
			IdeWrite(controller, channel, ATA_REG_LBA0, lba & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA1, (lba >> 8) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA2, (lba >> 16) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);
		}

	}
	else
	{
		UINT8 sector = (UINT8)(lba % drive->geometry.sector);
		UINT8 head = (UINT8)(lba / drive->geometry.sector % drive->geometry.cylinder);
		UINT16 cylinder = (UINT16)(lba / drive->geometry.sector / drive->geometry.cylinder);
		sector += 1;
		IdeWrite(controller, channel, ATA_REG_HDDEVSEL, 0xA0 | (isSlave << 4) | (head&(0xf))); // Drive & CHS
		IdeWrite(controller, channel, ATA_REG_SECCOUNT0, (UCHAR)numberOfSectors);
		IdeWrite(controller, channel, ATA_REG_LBA0, sector);
		IdeWrite(controller, channel, ATA_REG_LBA1, (UINT8) (cylinder & 0xFF));
		IdeWrite(controller, channel, ATA_REG_LBA2, (UINT8) ((cylinder >> 8) & 0xFF));
		IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);
	}

	for (i = 0; i < numberOfSectors; i++)
	{
		UINT8 err = IdePoll(controller, channel, 1);
		
		if (err & (ATA_SR_ERR || ATA_SR_DF))
		{
			PrintT("ERR: %X, %i/%i\n", lba, i + 1, (ULONG64)numberOfSectors);
			return err;
		}
		if (direction == 0)
		{
			for (j = 0; j < 256; j++)
			{
				*((UINT16*) (buffer + 2 * j)) = inw(bus);
			}
		}
		else
		{
			for (j = 0; j < 256; j++)
			{
				outw(bus, *((UINT16*) (buffer + 2 * j)));
			}
			IdeWrite(controller, channel, ATA_REG_COMMAND, (lba > 0xFFFFFFF) ? ATA_CMD_CACHE_FLUSH_EXT : ATA_CMD_CACHE_FLUSH);
		}
		buffer += 512;
	}

	IdePoll(controller, channel, 0);
	return 0;
}