#include "nnxint.h"
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
#include "../pe/pe.h" // from bootloader

VOID DebugX(UINT64 n) {
	PrintT("0x%X\n", n);
}

VOID DebugD(UINT64 n) {
	PrintT("%d\n", n);
}

VOID* gRDSP;

void LoadKernel() {
	MZ_FILE_TABLE MZFileTable;
	VFS* vfs = VfsGetPointerToVfs(0);
	VFS_FILE* file = vfs->Functions.OpenFile(vfs, "efi\\boot\\NNXOSKRN.EXE");
	PE_FILE_TABLE PEFileTable;
	DATA_DIRECTORY *dataDirectories;
	INT64 i;
	UINT64 imageBase;
	UINT64(*EntryPoint)(LdrKernelInitializationData*);

	if (file == 0) {
		PrintT("Error loading kernel.\n");
		return;
	}

	vfs->Functions.ReadFile(file, sizeof(MZFileTable), &MZFileTable);
	file->FilePointer = MZFileTable.e_lfanew;

	if (MZFileTable.signature != IMAGE_MZ_MAGIC) {
		PrintT("Invalid PE file %x %X\n", MZFileTable.signature, IMAGE_MZ_MAGIC);
		return;
	}

	vfs->Functions.ReadFile(file, sizeof(PEFileTable), &PEFileTable);

	if (PEFileTable.signature != IMAGE_PE_MAGIC) {
		PrintT("Invalid PE header\n");
		return;
	}

	dataDirectories = NNXAllocatorAlloc(sizeof(DATA_DIRECTORY) * PEFileTable.optionalHeader.NumberOfDataDirectories);
	vfs->Functions.ReadFile(file, sizeof(dataDirectories) * PEFileTable.optionalHeader.NumberOfDataDirectories, dataDirectories);
	NNXAllocatorFree(dataDirectories);

	imageBase = PEFileTable.optionalHeader.ImageBase;
	for (i = 0; i < PEFileTable.fileHeader.NumberOfSections; i++) {
		SECTION_HEADER sHeader;
		UINT64 tempFP, status;
		int j;

		if (status = vfs->Functions.ReadFile(file, sizeof(SECTION_HEADER), &sHeader))
			return;

		tempFP = file->FilePointer;
		file->FilePointer = sHeader.SectionPointer;

		if (status = vfs->Functions.ReadFile(file, sHeader.SizeOfSection, ((UINT64)sHeader.VirtualAddress) + imageBase))
			return;

		if (sHeader.VirtualSize > sHeader.SizeOfSection) {
			for (j = 0; j < sHeader.VirtualSize - sHeader.SizeOfSection; j++) {
				((UINT8*)(((UINT64)sHeader.VirtualAddress) + imageBase + sHeader.SizeOfSection))[j] = 0;
			}
		}

		PrintT("Read section %S to 0x%X\n", sHeader.Name, 8, ((UINT64)sHeader.VirtualAddress) + imageBase);

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

	EntryPoint = PEFileTable.optionalHeader.EntrypointRVA + imageBase;
	PrintT("Kernel at 0x%X\n", EntryPoint);
	EntryPoint(&data);
}


void IntTestASM();

const char version[] = " 0.1";

UINT32* gFramebuffer;
UINT32* gFramebufferEnd;
UINT32 gPixelsPerScanline;
UINT32 gWidth;
UINT32 gHeight;
extern BOOL gInteruptInitialized;

#ifdef BOCHS
void KernelMain(){
#else
void KernelMain(int* framebuffer, int* framebufferEnd, UINT32 width, UINT32 height, UINT32 pixelsPerScanline, UINT64(*ExitBootServices)(void*, UINT64), void* imageHandle, UINT64 n,
	UINT8* nnxMMap, UINT64 nnxMMapSize, UINT64 memorySize, VOID* rdsp) {
#endif
	UINT64 i;
	gInteruptInitialized = FALSE;
	gFramebuffer = framebuffer;
	gFramebufferEnd = framebufferEnd;
	gPixelsPerScanline = pixelsPerScanline;
	gWidth = width;
	gHeight = height;
	gRDSP = rdsp;

	void(*interrupts[])() = { Exception0, Exception1, Exception2, Exception3,
										Exception4, Exception5, Exception6, Exception7,
										Exception8, ExceptionReserved, Exception10, Exception11,
										Exception12, Exception13, Exception14, ExceptionReserved,
										Exception16, Exception17, Exception18, Exception19,
										Exception20, ExceptionReserved, ExceptionReserved, ExceptionReserved,
										ExceptionReserved, ExceptionReserved, ExceptionReserved, ExceptionReserved,
										ExceptionReserved, ExceptionReserved, Exception30, ExceptionReserved,
										IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7,
										IRQ8, IRQ9, IRQ10, IRQ11, IRQ12, IRQ13, IRQ14
	};

	ExitBootServices(imageHandle, n);
	DisableInterrupts();
	GlobalPhysicalMemoryMap = nnxMMap;
	GlobalPhysicalMemoryMapSize = nnxMMapSize;

	for(i = ((UINT64)GlobalPhysicalMemoryMap) / 4096; i < (((UINT64)GlobalPhysicalMemoryMap) + GlobalPhysicalMemoryMapSize + 4095) / 4096; i++){
		GlobalPhysicalMemoryMap[i] = MEM_TYPE_USED_PERM;
	}

	for (i = 0; i < FrameBufferSize() / PAGE_SIZE_SMALL + 1; i++) {
		GlobalPhysicalMemoryMap[((UINT64)gFramebuffer) / 4096 + i] = MEM_TYPE_USED_PERM;
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

	GDTR* gdtr = GDT_VIRTUAL_ADDRESS;
	GDT* gdt = GDT_VIRTUAL_ADDRESS + sizeof(GDTR);
	IDTR* idtr = IDT_VIRTUAL_ADDRESS;
	IDT* idt = IDT_VIRTUAL_ADDRESS + sizeof(IDTR);
	PagingMapPage(gdtr, InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM), PAGE_PRESENT | PAGE_WRITE);
	PagingMapPage(idtr, InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM), PAGE_PRESENT | PAGE_WRITE);

	gdtr->Size = sizeof(GDTEntry) * 5 - 1;
	gdtr->Base = gdt;

	idtr->Size = sizeof(IDTEntry) * 128 - 1;
	idtr->Base = idt;

	((UINT64*)gdt->Entries)[0] = 0;		//NULL DESCRIPTOR

	gdt->Entries[1].Base0To15 = 0;		//CODE, RING 0 DESCRIPTOR
	gdt->Entries[1].Base16To23 = 0;
	gdt->Entries[1].Base24To31 = 0;
	gdt->Entries[1].Limit0To15 = 0xFFFF;
	gdt->Entries[1].Limit16To19 = 0xF;
	gdt->Entries[1].Flags = 0xa;
	gdt->Entries[1].AccessByte = 0x9a;

	gdt->Entries[2].Base0To15 = 0;		//DATA, RING 0 DESCRIPTOR
	gdt->Entries[2].Base16To23 = 0;
	gdt->Entries[2].Base24To31 = 0;
	gdt->Entries[2].Limit0To15 = 0xFFFF;
	gdt->Entries[2].Limit16To19 = 0xF;
	gdt->Entries[2].Flags = 0xc;
	gdt->Entries[2].AccessByte = 0x92;

	gdt->Entries[3].Base0To15 = 0;	//CODE, RING 3 DESCRIPTOR
	gdt->Entries[3].Base16To23 = 0;
	gdt->Entries[3].Base24To31 = 0;
	gdt->Entries[3].Limit0To15 = 0xFFFF;
	gdt->Entries[3].Limit16To19 = 0xF;
	gdt->Entries[3].Flags = 0xa;
	gdt->Entries[3].AccessByte = 0xfa;

	gdt->Entries[4].Base0To15 = 0;		//DATA, RING 3 DESCRIPTOR
	gdt->Entries[4].Base16To23 = 0;
	gdt->Entries[4].Base24To31 = 0;
	gdt->Entries[4].Limit0To15 = 0xFFFF;
	gdt->Entries[4].Limit16To19 = 0xF;
	gdt->Entries[4].Flags = 0xc;
	gdt->Entries[4].AccessByte = 0xf2;
	LoadGDT(gdtr);

	for (int a = 0; a < 128; a++) {
		idt->Entries[a].selector = 0x8;
		void(*handler)();
		handler = IntTestASM;

		if (a < sizeof(interrupts) / sizeof(*interrupts))
			handler = interrupts[a];

		idt->Entries[a].offset0to15 = (UINT16)(((UINT64)handler) & 0xFFFF);
		idt->Entries[a].offset16to31 = (UINT16)((((UINT64)handler) >> 16) & 0xFFFF);
		idt->Entries[a].offset32to63 = (UINT32)((((UINT64)handler) >> 32) & 0xFFFFFFFF);

		idt->Entries[a].type = 0x8E;
		idt->Entries[a].ist = 0;
	}

	LoadIDT(idtr);
	/* TODO: Implement page-fault handler capable of dealing with page invalidation if a faulty address is mapped in the paging structures, but not in the TLB */
	gInteruptInitialized = TRUE;
	
	KeyboardInitialize();
	PrintT("Keyboard initialized.\n");
	PicInitialize();

	NNXAllocatorInitialize();
	for (i = 0; i < 8; i++) {
		NNXAllocatorAppend(PagingAllocatePage(), 4096);
	}

	UINT8 status = 0;

	TextIoClear();
	TextIoSetCursorPosition(0, 20);
	PrintT("NNXOSLDR.exe version %s\n", version);
	PrintT("Stage 2 loaded... %x %x %i\n", framebuffer, framebufferEnd, (((UINT64)framebufferEnd) - ((UINT64)framebuffer)) / 4096);

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
	PitUniprocessorInitialize();

	EnableInterrupts();
	
	LoadKernel();
	
	while (1);
}
