#ifndef NNX_LDR_HEADER
#define NNX_LDR_HEADER

#include <nnxtype.h>

typedef struct KLdrKernelInitializationData {
	UINT32* Framebuffer;
	UINT32* FramebufferEnd;
	UINT32 FramebufferWidth;
	UINT32 FramebufferHeight;
	UINT32 FramebufferPixelsPerScanline;
	UINT8* PhysicalMemoryMap;
	UINT64 PhysicalMemoryMapSize;
	UINT64**** PML4;
	UINT64**** PML4_IdentifyMap;
	VOID(*DebugX)(UINT64);
	VOID(*DebugD)(UINT64);
#ifdef NNX_AML_HEADER
	ACPI_RDSP* rdsp;
#else
	VOID* rdsp;
#endif
}LdrKernelInitData, LdrKernelInitializationData;

#endif