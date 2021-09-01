#include "HAL/PCI/PCIIDE.h"
#include "video/SimpleTextIo.h"
#include "HAL/Port.h"
#include "device/hdd/hdd.h"

#define WAIT_MACRO for (int __ = 0; __ < 10000; __++);

VOID IdeWrite(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg, UINT8 data) {
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, 0x80 | pic->channels[channel].no_interrupt);
	if (reg < 0x08)
		outb(pic->channels[channel].base + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(pic->channels[channel].base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(pic->channels[channel].ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(pic->channels[channel].bus_master_ide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, pic->channels[channel].no_interrupt);
}

UINT8 IdeRead(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg) {
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, 0x80 | pic->channels[channel].no_interrupt);
	unsigned char result;
	if (reg < 0x08)
		result = inb(pic->channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(pic->channels[channel].base + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(pic->channels[channel].ctrl + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(pic->channels[channel].bus_master_ide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		IdeWrite(pic, channel, ATA_REG_CONTROL, pic->channels[channel].no_interrupt);
	return result;
}

VOID FillBars(PCI_IDE_CONTROLLER* pic) {
	pic->BAR0 = PciGetBar0(pic->busNumber, pic->deviceNumber, pic->functionNumber);
	pic->BAR1 = PciGetBar1(pic->busNumber, pic->deviceNumber, pic->functionNumber);
	pic->BAR2 = PciGetBar2(pic->busNumber, pic->deviceNumber, pic->functionNumber);
	pic->BAR3 = PciGetBar3(pic->busNumber, pic->deviceNumber, pic->functionNumber);
	pic->BAR4 = PciGetBar4(pic->busNumber, pic->deviceNumber, pic->functionNumber);
	pic->channels[0].base = (pic->BAR0 & 0xfffffffc) + 0x1F0 * (!pic->BAR0);
	pic->channels[0].ctrl = (pic->BAR1 & 0xfffffffc) + 0x3F6 * (!pic->BAR1);
	pic->channels[1].base = (pic->BAR2 & 0xfffffffc) + 0x170 * (!pic->BAR2);
	pic->channels[1].ctrl = (pic->BAR3 & 0xfffffffc) + 0x376 * (!pic->BAR3);
	pic->channels[0].bus_master_ide = (pic->BAR4 & 0xfffffffc);
	pic->channels[1].bus_master_ide = (pic->BAR4 & 0xfffffffc) + 8;
}


VOID IdeReadBuffer(PCI_IDE_CONTROLLER* pic, UINT8 channel, UINT8 reg, UINT64 buffer, UINT32 quads) {

	if (reg < 0x08)
		DiskReadLong(pic->channels[channel].base + reg - 0x00, buffer, quads);
	else if (reg < 0x0C)
		DiskReadLong(pic->channels[channel].base + reg - 0x06, buffer, quads);
	else if (reg < 0x0E)
		DiskReadLong(pic->channels[channel].ctrl + reg - 0x0A, buffer, quads);
	else if (reg < 0x16)
		DiskReadLong(pic->channels[channel].bus_master_ide + reg - 0x0E, buffer, quads);

}

VOID SearchForDevices(PCI_IDE_CONTROLLER* pic) {

	int diskCount = 0;

	IdeWrite(pic, 0, ATA_REG_CONTROL, 2);
	IdeWrite(pic, 1, ATA_REG_CONTROL, 2);

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			pic->drives[diskCount].Reserved = 0;
			IdeWrite(pic, i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
			WAIT_MACRO;

			IdeWrite(pic, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			WAIT_MACRO;

			UINT8 type = 0;
			UINT8 status = IdeRead(pic, i, ATA_REG_STATUS);
			if (status == 0)
				continue;
			UINT8 error = 0;

			while (1) {
				status = IdeRead(pic, i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR)) { error = 1; break; }
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			if (error != 0) {
				unsigned char LBA1 = IdeRead(pic, i, ATA_REG_LBA1);
				unsigned char LBA2 = IdeRead(pic, i, ATA_REG_LBA2);

				if (LBA1 == 0x14 && LBA2 == 0xEB)
					type = 1;
				else if (LBA1 == 0x69 && LBA2 == 0x96)
					type = 1;
				else
					continue;

				IdeWrite(pic, i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				status = IdeRead(pic, i, ATA_REG_STATUS);
				error = status & ATA_SR_ERR;
				WAIT_MACRO;
			}
			
			IdeReadBuffer(pic, i, ATA_REG_DATA, pic->ide_buffer, 128);

			pic->drives[diskCount].Reserved = 1;
			pic->drives[diskCount].type = type;
			pic->drives[diskCount].channel = i;
			pic->drives[diskCount].drive = j;
			pic->drives[diskCount].signature = *((UINT16*)(pic->ide_buffer + ATA_IDENT_DEVICETYPE));
			pic->drives[diskCount].capabilities = *((UINT16*)(pic->ide_buffer + ATA_IDENT_CAPABILITIES));
			pic->drives[diskCount].commandSets = *((UINT32*)(pic->ide_buffer + ATA_IDENT_COMMANDSETS));

			pic->drives[diskCount].geometry.sector = *((UINT8*)(pic->ide_buffer + ATA_IDENT_SECTORS));
			pic->drives[diskCount].geometry.head = *((UINT8*)(pic->ide_buffer + ATA_IDENT_HEADS));
			pic->drives[diskCount].geometry.cylinder = *((UINT16*)(pic->ide_buffer + ATA_IDENT_CYLINDERS));

			if (pic->drives[diskCount].commandSets & (1 << 26))
				pic->drives[diskCount].size = *((unsigned int *)(pic->ide_buffer + ATA_IDENT_MAX_LBA_EXT));
			else
				pic->drives[diskCount].size = *((unsigned int *)(pic->ide_buffer + ATA_IDENT_MAX_LBA));

			pic->drives[diskCount].size *= 512;

			for (int k = 0; k < 40; k += 2) {
				pic->drives[diskCount].model[k] = pic->ide_buffer[ATA_IDENT_MODEL + k + 1];
				pic->drives[diskCount].model[k + 1] = pic->ide_buffer[ATA_IDENT_MODEL + k];
			}
			pic->drives[diskCount].model[40] = 0;
			pic->drives[diskCount].controller = 0; //set the controller to nullptr for unused drives to avoid using them
			diskCount++;
		}
	}

}

PCI_IDE_CONTROLLER PciIdeInitPciDevice(UINT8 bus, UINT8 device, UINT8 function, UINT8 progIf) {
	PCI_IDE_CONTROLLER pic = { 0 };
	pic.busNumber = bus;
	pic.deviceNumber = device;
	pic.functionNumber = function;
	pic.progIf = progIf;

	pic.interrupt_number = PciConfigReadWord(bus, device, function, 0x3c) & 0xff;
	PciConfigWriteByte(bus, device, function, 0x3f, 0xfe);
	pic.interrupt_number = PciConfigReadWord(bus, device, function, 0x3c) & 0xff;

	if (pic.interrupt_number == 0xFE) {
		PciConfigWriteByte(bus, device, function, 0x3f, 14);
		pic.interrupt_number = 14;
	}
	else {
		if (progIf == 0x80 || progIf == 0x8A) { //parallel IDE
			pic.interrupt_number = 210;
		}
	}

	FillBars(&pic);
	return pic;
}

UINT8 IdePoll(PCI_IDE_CONTROLLER* controller, UINT8 channel, BOOL setError) {
	for (int i = 0; i < 4; i++) {
		IdeRead(controller, channel, ATA_REG_ALTSTATUS);
	}

	WAIT_MACRO;
	
	UINT8 status;
	while (status = IdeRead(controller, channel, ATA_REG_STATUS)) {
		if (!(status & ATA_SR_BSY))
			break;
	}
	
	if (setError) {
		
		if (status & ATA_SR_ERR) {
			PrintTA("ERR\n");
			return status;
		}
		if (status & ATA_SR_DF) {
			PrintTA("DF\n");
			return status;
		}
		if (!(status & ATA_SR_DRQ)) {
			PrintTA("DRQ\n");

			return status;
		}
	}
	return 0;
}

UINT64 PciIdeDiskIo(IDE_DRIVE* drive, UINT8 direction, UINT64 lba, UINT16 numberOfSectors, UINT8* buffer) {
	UINT8 channel = drive->channel;
	BOOL isSlave = drive->drive;
	PCI_IDE_CONTROLLER* controller = drive->controller;
	UINT16 bus = controller->channels[channel].base;
	controller->channels[channel].no_interrupt = 1;
	IdeWrite(controller, channel, ATA_REG_CONTROL, 0x02);

	while (IdeRead(controller, channel, ATA_REG_STATUS) & ATA_SR_BSY);

	if (drive->capabilities & IDE_LBA_SUPPORT) {
		IdeWrite(controller, channel, ATA_REG_HDDEVSEL, 0xE0 | (isSlave << 4) | ((lba > 0xfffffff) ? 0 : ((lba & 0xF000000) >> 24))); // Drive & LBA
		if (lba > 0xfffffff) {
			
			IdeWrite(controller, channel, ATA_REG_SECCOUNT1, (numberOfSectors&0xFF00)>>8);
			IdeWrite(controller, channel, ATA_REG_LBA3, (lba >> 24) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA4, (lba >> 32) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA5, (lba >> 40) & 0x0F);
		
			IdeWrite(controller, channel, ATA_REG_SECCOUNT0, numberOfSectors);
			IdeWrite(controller, channel, ATA_REG_LBA0, lba & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA1, (lba >> 8) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA2, (lba >> 16) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO_EXT : ATA_CMD_READ_PIO_EXT);
		}
		else {
			IdeWrite(controller, channel, ATA_REG_SECCOUNT0, numberOfSectors);
			IdeWrite(controller, channel, ATA_REG_LBA0, lba & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA1, (lba >> 8) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_LBA2, (lba >> 16) & 0xFF);
			IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);
		}
		
	}
	else {
		UINT8 sector = lba % drive->geometry.sector;
		UINT8 head = lba / drive->geometry.sector % drive->geometry.cylinder;
		UINT16 cylinder = lba / drive->geometry.sector / drive->geometry.cylinder;
		sector += 1;
		IdeWrite(controller, channel, ATA_REG_HDDEVSEL, 0xA0 | (isSlave << 4) | (head&(0xf))); // Drive & CHS
		IdeWrite(controller, channel, ATA_REG_SECCOUNT0, numberOfSectors);
		IdeWrite(controller, channel, ATA_REG_LBA0, sector);
		IdeWrite(controller, channel, ATA_REG_LBA1, (UINT8)(cylinder & 0xFF));
		IdeWrite(controller, channel, ATA_REG_LBA2, (UINT8)((cylinder >> 8) & 0xFF));
		IdeWrite(controller, channel, ATA_REG_COMMAND, direction ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO);
	}
	
	for (int i = 0; i < numberOfSectors; i++) {
		UINT8 err = IdePoll(controller, channel, 1);
		if (err & (ATA_SR_ERR || ATA_SR_DF)) {
			PrintT("ERR: %X, %i/%i\n", lba, i + 1, numberOfSectors);
			return err;
		}
		if (direction == 0) {
			for (int i = 0; i < 256; i++) {
				*((UINT16*)(buffer+2*i)) = inw(bus);
			}
		}
		else {
			for (int i = 0; i < 256; i++) {
				outw(bus, *((UINT16*)(buffer + 2*i)));
			}
			IdeWrite(controller, channel, ATA_REG_COMMAND, (lba > 0xfffffff) ? ATA_CMD_CACHE_FLUSH_EXT : ATA_CMD_CACHE_FLUSH);
		}
		buffer += 512;
	}

	IdePoll(controller, channel, 0);
	return 0;
}