#include "cpu.h"
#include "pcr.h"
#include <HAL/registers.h>

VOID HalSetPcr(PKPCR pcr)
{
	HalX64WriteMsr(0xC0000102UL, NULL);
	HalX64WriteMsr(0xC0000101UL, pcr);
}

PKPCR HalSwapInPcr()
{
	return HalX64SwapGs();
}
