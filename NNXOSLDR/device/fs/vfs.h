#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include "../../HAL/PCI/PCIIDE.h"
#include "../../HAL/PCI/PCI.h"
#define VFS_MAX_NUMBER 64

typedef struct VirtualFileSystemOperations {
	void (*todo)();
}VirtualFileSystemOperations, VFSOperations;

typedef struct VirtualFileSystem {
	IDEDrive* drive;
	UINT64 lbaStart;
	UINT64 sizeInSectors;
	UINT64 allocationUnitSize;
	VirtualFileSystemOperations operations;
}VirtualFileSystem, VFS;

void VFSInit();
unsigned int VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize);
VirtualFileSystem* VFSGetPointerToVFS(unsigned int n);
UINT64 VFSReadSector(VirtualFileSystem*, UINT64 n, BYTE* destination);

#endif