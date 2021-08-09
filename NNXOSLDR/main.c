#include "nnxint.h"
#include "video/SimpleTextIO.h"
#include "memory/physical_allocator.h"
#include "memory/paging.h"
#include "HAL/GDT.h"
#include "HAL/IDT.h"
#include "HAL/PIC.h"
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
	VFS* vfs = VFSGetPointerToVFS(0);
	VFSFile* file = vfs->functions.OpenFile(vfs, "efi\\boot\\NNXOSKRN.EXE");
	PE_FILE_TABLE PEFileTable;
	DATA_DIRECTORY *dataDirectories;
	INT64 i;
	UINT64 imageBase;
	UINT64(*EntryPoint)(LdrKernelInitializationData*);

	if (file == 0) {
		PrintT("Error loading kernel.\n");
		return;
	}

	vfs->functions.ReadFile(file, sizeof(MZFileTable), &MZFileTable);
	file->filePointer = MZFileTable.e_lfanew;

	if (MZFileTable.signature != IMAGE_MZ_MAGIC) {
		PrintT("Invalid PE file %x %X\n", MZFileTable.signature, IMAGE_MZ_MAGIC);
		return;
	}

	vfs->functions.ReadFile(file, sizeof(PEFileTable), &PEFileTable);

	if (PEFileTable.signature != IMAGE_PE_MAGIC) {
		PrintT("Invalid PE header\n");
		return;
	}

	dataDirectories = NNXAllocatorAlloc(sizeof(DATA_DIRECTORY) * PEFileTable.optionalHeader.NumberOfDataDirectories);
	vfs->functions.ReadFile(file, sizeof(dataDirectories) * PEFileTable.optionalHeader.NumberOfDataDirectories, dataDirectories);
	NNXAllocatorFree(dataDirectories);

	imageBase = PEFileTable.optionalHeader.ImageBase;
	for (i = 0; i < PEFileTable.fileHeader.NumberOfSections; i++) {
		SECTION_HEADER sHeader;
		UINT64 tempFP, status;
		int j;

		if (status = vfs->functions.ReadFile(file, sizeof(SECTION_HEADER), &sHeader))
			return;

		tempFP = file->filePointer;
		file->filePointer = sHeader.SectionPointer;

		if (status = vfs->functions.ReadFile(file, sHeader.SizeOfSection, ((UINT64)sHeader.VirtualAddress) + imageBase))
			return;

		if (sHeader.VirtualSize > sHeader.SizeOfSection) {
			for (j = 0; j < sHeader.VirtualSize - sHeader.SizeOfSection; j++) {
				((UINT8*)(((UINT64)sHeader.VirtualAddress) + imageBase + sHeader.SizeOfSection))[j] = 0;
			}
		}

		PrintT("Read section %S to 0x%X\n", sHeader.Name, 8, ((UINT64)sHeader.VirtualAddress) + imageBase);

		file->filePointer = tempFP;
	}

	vfs->functions.CloseFile(file);

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
	EntryPoint(&data); /* TODO: This crashes */
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

	MemorySize = memorySize;

	TextIOInitialize(framebuffer, framebufferEnd, width, height, pixelsPerScanline);
	UINT32 box[] = { 0, width, 20, height - 20 };
	TextIOSetBoundingBox(box);
	TextIOSetColorInformation(0xFFFFFFFF, 0x00000000, 0);
	TextIOClear();
	TextIOSetCursorPosition(0, 20);

	PrintT("Initializing memory\n");
	
	PagingInit();

	GDTR* gdtr = GDT_VIRTUAL_ADDRESS;
	GDT* gdt = GDT_VIRTUAL_ADDRESS + sizeof(GDTR);
	IDTR* idtr = IDT_VIRTUAL_ADDRESS;
	IDT* idt = IDT_VIRTUAL_ADDRESS + sizeof(IDTR);
	PagingMapPage(gdtr, InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM), PAGE_PRESENT | PAGE_WRITE);
	PagingMapPage(idtr, InternalAllocatePhysicalPageWithType(MEM_TYPE_USED_PERM), PAGE_PRESENT | PAGE_WRITE);

	gdtr->size = sizeof(GDTEntry) * 5 - 1;
	gdtr->offset = gdt;

	idtr->size = sizeof(IDTEntry) * 128 - 1;
	idtr->offset = idt;

	((UINT64*)gdt->entries)[0] = 0;		//NULL DESCRIPTOR

	gdt->entries[1].base0to15 = 0;		//CODE, RING 0 DESCRIPTOR
	gdt->entries[1].base16to23 = 0;
	gdt->entries[1].base24to31 = 0;
	gdt->entries[1].limit0to15 = 0xFFFF;
	gdt->entries[1].limit16to19 = 0xF;
	gdt->entries[1].flags = 0xa;
	gdt->entries[1].accessByte = 0x9a;

	gdt->entries[2].base0to15 = 0;		//DATA, RING 0 DESCRIPTOR
	gdt->entries[2].base16to23 = 0;
	gdt->entries[2].base24to31 = 0;
	gdt->entries[2].limit0to15 = 0xFFFF;
	gdt->entries[2].limit16to19 = 0xF;
	gdt->entries[2].flags = 0xc;
	gdt->entries[2].accessByte = 0x92;

	gdt->entries[3].base0to15 = 0;	//CODE, RING 3 DESCRIPTOR
	gdt->entries[3].base16to23 = 0;
	gdt->entries[3].base24to31 = 0;
	gdt->entries[3].limit0to15 = 0xFFFF;
	gdt->entries[3].limit16to19 = 0xF;
	gdt->entries[3].flags = 0xa;
	gdt->entries[3].accessByte = 0xfa;

	gdt->entries[4].base0to15 = 0;		//DATA, RING 3 DESCRIPTOR
	gdt->entries[4].base16to23 = 0;
	gdt->entries[4].base24to31 = 0;
	gdt->entries[4].limit0to15 = 0xFFFF;
	gdt->entries[4].limit16to19 = 0xF;
	gdt->entries[4].flags = 0xc;
	gdt->entries[4].accessByte = 0xf2;
	LoadGDT(gdtr);

	for (int a = 0; a < 128; a++) {
		idt->entries[a].selector = 0x8;
		void(*handler)();
		handler = IntTestASM;

		if (a < sizeof(interrupts) / sizeof(*interrupts))
			handler = interrupts[a];

		idt->entries[a].offset0to15 = (UINT16)(((UINT64)handler) & 0xFFFF);
		idt->entries[a].offset16to31 = (UINT16)((((UINT64)handler) >> 16) & 0xFFFF);
		idt->entries[a].offset32to63 = (UINT32)((((UINT64)handler) >> 32) & 0xFFFFFFFF);

		idt->entries[a].type = 0x8E;
		idt->entries[a].ist = 0;
	}

	LoadIDT(idtr);
	/* TODO: Implement page-fault handler capable of dealing with page invalidation if a faulty address is mapped in the paging structures, but not in the TLB */
	gInteruptInitialized = TRUE;
	
	KeyboardInitialize();
	PrintT("Keyboard initialized.\n");
	PICInitialize();


	NNXAllocatorInitialize();
	PrintT("NNXAllocator Initialization\n");
	for (int a = 0; a < 8; a++) {
		NNXAllocatorAppend(PagingAllocatePage(), 4096);
	}

	UINT8 status = 0;

	TextIOClear();
	TextIOSetCursorPosition(0, 20);
	PrintT("NNXOSLDR.exe version %s\n", version);
	PrintT("Stage 2 loaded... %x %x %i\n", framebuffer, framebufferEnd, (((UINT64)framebufferEnd) - ((UINT64)framebuffer)) / 4096);

	PrintT("Memory map: ");
	TextIOSetColorInformation(0xFFFFFFFF, 0xFF007F00, 1);
	PrintT(" FREE ");
	TextIOSetColorInformation(0xFFFFFFFF, 0xFF7F0000, 1);
	PrintT(" USED ");
	TextIOSetColorInformation(0xFF000000, 0xFFAFAF00, 1);
	PrintT(" UTIL ");
	TextIOSetColorInformation(0xFF000000, 0xFF00FFFF, 1);
	PrintT(" PERM ");
	TextIOSetColorInformation(0xFF000000, 0xFFFF00FF, 1);
	PrintT(" KERNEL ");
	TextIOSetColorInformation(0xFFFFFFFF, 0, 1);

	DrawMap();

	TextIOSetCursorPosition(0, 220);

	VFSInit();
	PCIScan();

	EnableInterrupts();

	NNXLoggerTest(VFSGetPointerToVFS(0));
	
	LoadKernel();
	
	while (1);
}
