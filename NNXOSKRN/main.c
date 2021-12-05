#include <nnxtype.h>
#include "video/SimpleTextIo.h"
#include "memory/physical_allocator.h"
#include "memory/paging.h"
#include "HAL/GDT.h"
#include "HAL/IDT.h"
#include "HAL/PIC.h"
#include "HAL/PIT.h"
#include "device/Keyboard.h"
#include "HAL/PCI/PCI.h"
#include "memory/nnxalloc.h"
#include "device/fs/vfs.h"
#include "nnxlog.h"
#include "nnxcfg.h"
#include "nnxoskldr.h"
#include <nnxpe.h>

/* ignore, remove when sorting the 3 stage mess out */
VOID ApicSendEoi()
{
}

VOID DebugX(UINT64 n)
{
	PrintT("0x%X\n", n);
}

VOID DebugD(UINT64 n)
{
	PrintT("%d\n", n);
}

VOID* gRDSP;

void LoadKernel()
{
	MZ_FILE_TABLE MZFileTable;
	VFS* vfs = VfsGetPointerToVfs(0);
	VFS_FILE* file = vfs->Functions.OpenFile(vfs, "efi\\boot\\NNXOSKRN.EXE");
	PE_FILE_TABLE PEFileTable;
	DATA_DIRECTORY *dataDirectories;
	INT64 i;
	UINT64 imageBase;
	UINT64(*EntryPoint)(LdrKernelInitializationData*);

	if (file == 0)
	{
		PrintT("Error loading kernel.\n");
		return;
	}

	vfs->Functions.ReadFile(file, sizeof(MZFileTable), &MZFileTable);
	file->FilePointer = MZFileTable.e_lfanew;

	if (MZFileTable.Signature != IMAGE_MZ_MAGIC)
	{
		PrintT("Invalid PE file %x %X\n", MZFileTable.Signature, IMAGE_MZ_MAGIC);
		return;
	}

	vfs->Functions.ReadFile(file, sizeof(PEFileTable), &PEFileTable);

	if (PEFileTable.Signature != IMAGE_PE_MAGIC)
	{
		PrintT("Invalid PE header\n");
		return;
	}

	dataDirectories = NNXAllocatorAlloc(sizeof(DATA_DIRECTORY) * PEFileTable.OptionalHeader.NumberOfDataDirectories);
	vfs->Functions.ReadFile(file, sizeof(dataDirectories) * PEFileTable.OptionalHeader.NumberOfDataDirectories, dataDirectories);
	NNXAllocatorFree(dataDirectories);

	imageBase = PEFileTable.OptionalHeader.ImageBase;
	for (i = 0; i < PEFileTable.FileHeader.NumberOfSections; i++)
	{
		SECTION_HEADER sHeader;
		UINT64 tempFP, status;
		int j;

		if (status = vfs->Functions.ReadFile(file, sizeof(SECTION_HEADER), &sHeader))
			return;

		tempFP = file->FilePointer;
		file->FilePointer = sHeader.PointerToDataRVA;

		if (status = vfs->Functions.ReadFile(file, sHeader.SizeOfSection, ((UINT64) sHeader.VirtualAddressRVA) + imageBase))
			return;

		if (sHeader.VirtualSize > sHeader.SizeOfSection)
		{
			for (j = 0; j < sHeader.VirtualSize - sHeader.SizeOfSection; j++)
			{
				((UINT8*) (((UINT64) sHeader.VirtualAddressRVA) + imageBase + sHeader.SizeOfSection))[j] = 0;
			}
		}

		PrintT("Read section %S to 0x%X\n", sHeader.Name, 8, ((UINT64) sHeader.VirtualAddressRVA) + imageBase);

		file->FilePointer = tempFP;
	}

	vfs->Functions.CloseFile(file);

	LdrKernelInitializationData data;
	data.Framebuffer = gFramebuffer;
	data.FramebufferEnd = gFramebufferEnd;
	data.FramebufferHeight = gHeight;
	data.FramebufferWidth = gWidth;
	data.FramebufferPixelsPerScanline = gPixelsPerScanline;
	data.PhysicalMemoryMap = GlobalPhysicalMemoryMap;
	data.PhysicalMemoryMapSize = GlobalPhysicalMemoryMapSize;
	data.DebugD = DebugD;
	data.DebugX = DebugX;
	data.rdsp = gRDSP;

	EntryPoint = PEFileTable.OptionalHeader.EntrypointRVA + imageBase;
	PrintT("Kernel at 0x%X\n", EntryPoint);
	EntryPoint(&data);
}

const char version[] = " 0.1";

UINT32* gFramebuffer;
UINT32* gFramebufferEnd;
UINT32 gPixelsPerScanline;
UINT32 gWidth;
UINT32 gHeight;
extern BOOL HalpInteruptInitialized;

#ifdef BOCHS
void KernelMain()
{
#else
void KernelMain(int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, UINT32 pixelsPerScanline, UINT64(*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n,
				UINT8* nnxMMap, UINT64 nnxMMapSize, UINT64 memorySize, VOID* rdsp)
{
#endif
	UINT64 i;
	HalpInteruptInitialized = FALSE;
	gFramebuffer = framebuffer;
	gFramebufferEnd = framebufferEnd;
	gPixelsPerScanline = pixelsPerScanline;
	gWidth = width;
	gHeight = height;
	gRDSP = rdsp;

	ExitBootServices(imageHandle, n);
	DisableInterrupts();
	GlobalPhysicalMemoryMap = nnxMMap;
	GlobalPhysicalMemoryMapSize = nnxMMapSize;

	for (i = ((UINT64) GlobalPhysicalMemoryMap) / 4096; i < (((UINT64) GlobalPhysicalMemoryMap) + GlobalPhysicalMemoryMapSize + 4095) / 4096; i++)
	{
		GlobalPhysicalMemoryMap[i] = MEM_TYPE_USED_PERM;
	}

	for (i = 0; i < FrameBufferSize() / PAGE_SIZE_SMALL + 1; i++)
	{
		GlobalPhysicalMemoryMap[((UINT64) gFramebuffer) / 4096 + i] = MEM_TYPE_USED_PERM;
	}

	MemorySize = memorySize;

	TextIoInitialize(framebuffer, framebufferEnd, width, height, pixelsPerScanline);
	UINT32 box[] = { 0, width, 20, height - 20 };
	TextIoSetBoundingBox(box);
	TextIoSetColorInformation(0xFFFFFFFF, 0x00000000, 0);
	TextIoClear();
	TextIoSetCursorPosition(0, 20);

	PrintT("Initializing memory\n");

	PagingInit();

	HalpAllocateAndInitializeGdt();
	HalpAllocateAndInitializeIdt();

	KeyboardInitialize();
	PrintT("Keyboard initialized.\n");
	PicInitialize();

	NNXAllocatorInitialize();
	for (i = 0; i < 8; i++)
	{
		NNXAllocatorAppend(PagingAllocatePage(), 4096);
	}

	UINT8 status = 0;

	TextIoClear();
	TextIoSetCursorPosition(0, 20);
	PrintT("NNXOSLDR.exe version %s\n", version);
	PrintT("Stage 2 loaded... %x %x %i\n", framebuffer, framebufferEnd, (((UINT64) framebufferEnd) - ((UINT64) framebuffer)) / 4096);

	PrintT("Memory map: ");
	TextIoSetColorInformation(0xFFFFFFFF, 0xFF007F00, 1);
	PrintT(" FREE ");
	TextIoSetColorInformation(0xFFFFFFFF, 0xFF7F0000, 1);
	PrintT(" USED ");
	TextIoSetColorInformation(0xFF000000, 0xFFAFAF00, 1);
	PrintT(" UTIL ");
	TextIoSetColorInformation(0xFF000000, 0xFF00FFFF, 1);
	PrintT(" PERM ");
	TextIoSetColorInformation(0xFFFFFFFF, 0, 1);

	DrawMap();

	TextIoSetCursorPosition(0, 220);

	VfsInit();
	PciScan();

	EnableInterrupts();

	LoadKernel();

	while (1);
}
