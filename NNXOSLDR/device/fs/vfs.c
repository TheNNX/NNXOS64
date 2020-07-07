#include "vfs.h"
#include "../../HAL/PCI/PCIIDE.h"

VirtualFileSystem virtualFileSystems[VFS_MAX_NUMBER];

void VFSInit() {
	for (int i = 0; i < VFS_MAX_NUMBER; i++)
		virtualFileSystems[i].drive = 0;
}

unsigned int VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize) {
	int found = -1;
	
	for (int i = 0; (i < VFS_MAX_NUMBER) && (found == -1); i++) {
		if (virtualFileSystems[i].drive == 0) {
			virtualFileSystems[i].drive = drive;
			virtualFileSystems[i].lbaStart = lbaStart;
			virtualFileSystems[i].sizeInSectors = partitionSize;
			found = i;
		}
	}

	PrintT("Allocated VFS id %x\n", found);
	return found;
}

void VFSReadSector(VirtualFileSystem* vfs, BYTE* destination) {
	PCI_IDE_DiskIO(vfs->drive, 0, vfs->lbaStart, 1, destination);
}

VirtualFileSystem* VFSGetPointerToVFS(unsigned int n) {
	return virtualFileSystems + n;
}

