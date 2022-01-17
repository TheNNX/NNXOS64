#include "cpu.h"
#include "pcr.h"
#include "registers.h"

VOID HalSetPcr(PKPCR pcr)
{
	HalX64WriteMsr(0xC0000102UL, (UINT64)NULL);
	HalX64WriteMsr(0xC0000101UL, (UINT64)pcr);
}

PKPCR HalSwapInPcr()
{
	return HalX64SwapGs();
}
