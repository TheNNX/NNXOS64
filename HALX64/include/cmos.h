#ifndef NNX_CMOS_HEADER
#define NNX_CMOS_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

extern BOOLEAN HalNmiDesiredState;

NTHALAPI
VOID 
NTAPI
CmosInitialize(UINT8 century);

#ifdef NNX_HAL
VOID CmosWriteRegister(UCHAR Register,
                       UCHAR Value);

UCHAR CmosReadRegister(UCHAR Register);
#endif

#ifdef __cplusplus
}
#endif

#endif