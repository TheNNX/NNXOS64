#ifndef NNX_VFS_HEADER
#define NNX_VFS_HEADER

#include "../../HAL/PCI/PCIIDE.h"
#include "../../HAL/PCI/PCI.h"
#define VFS_MAX_NUMBER 64

typedef struct VirtualFileSystem {
	IDEDrive* drive;
	UINT64 lbaStart;
	UINT64 sizeInSectors;
	UINT64 allocationUnitSize;
}VirtualFileSystem, VFS;

#define VFS_ERR_INVALID_FILENAME	0xFFFFFFF1
#define VFS_ERR_INVALID_PATH		0xFFFFFFF2
#define VFS_ERR_INACCESSIBLE		0xFFFFFFF3
#define VFS_ERR_EOF					0xFFFFFFF3
#define VFS_ERR_NOT_A_DIRECTORY		0xFFFFFFF4
#define VFS_ERR_NOT_A_FILE			0xFFFFFFF5
#define VFS_ERR_FILE_NOT_FOUND		0xFFFFFFF6

void VFSInit();
unsigned int VFSAddPartition(IDEDrive* drive, UINT64 lbaStart, UINT64 partitionSize);
VirtualFileSystem* VFSGetPointerToVFS(unsigned int n);
UINT64 VFSReadSector(VirtualFileSystem*, UINT64 n, BYTE* destination);

#endif