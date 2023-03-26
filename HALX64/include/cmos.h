#ifndef NNX_CMOS_HEADER
#define NNX_CMOS_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern BOOLEAN HalNmiDesiredState;

    VOID CmosInitialize();

    VOID CmosWriteRegister(
        UCHAR Register,
        UCHAR Value
    );

    UCHAR CmosReadRegister(
        UCHAR Register
    );

#ifdef __cplusplus
}
#endif

#endif