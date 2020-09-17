#include "vfs.h"
#include "../../HAL/PCI/PCIIDE.h"

VirtualFileSystem virtualFileSystems[VFS_MAX_NUMBER];

void VFSInit() {
	VFS empty = { 0 };
	for (int i = 0; i < VFS_MAX_NUMBER; i++)
		virtualFileSystems[i] = empty;
}

UINT32 VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize) {
	UINT32 found = -1;
	
	for (UINT32 i = 0; (i < VFS_MAX_NUMBER) && (found == -1); i++) {
		if (virtualFileSystems[i].drive == 0) {
			virtualFileSystems[i].drive = drive;
			virtualFileSystems[i].lbaStart = lbaStart;
			virtualFileSystems[i].sizeInSectors = partitionSize;
			found = i;
		}
	}

	return found;
}

UINT64 VFSReadSector(VirtualFileSystem* vfs, UINT64 n, BYTE* destination) {
	return PCI_IDE_DiskIO(vfs->drive, 0, vfs->lbaStart + n, 1, destination);
}

UINT64 VFSWriteSector(VirtualFileSystem* vfs, UINT64 n, BYTE* source) {
	return PCI_IDE_DiskIO(vfs->drive, 1, vfs->lbaStart + n, 1, source);
}

VirtualFileSystem* VFSGetPointerToVFS(unsigned int n) {
	return virtualFileSystems + n;
}

