#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include "../../HAL/PCI/PCIIDE.h"
#include "../../HAL/PCI/PCI.h"

typedef struct VirtualFileSystemOperations {
	void (*todo)();
}VirtualFileSuystemOperations, VFSOperations;

typedef struct VirtualFileSystem {
	IDEDrive* drive;
	UINT64 lbaStart;
	UINT64 sizeInSectors;
	VirtualFileSuystemOperations operations;
}VirtualFileSystem, VFS;

#endif