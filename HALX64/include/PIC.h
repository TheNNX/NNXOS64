#ifndef NNX_PIC_HEADER
#define NNX_PIC_HEADER
#define PIC_PRIMARY 0x20
#define PIC_SECONDARY 0xA0

#define PIC_PRIMARY_DATA PIC_PRIMARY+1
#define PIC_SECONDARY_DATA PIC_SECONDARY+1

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(NNX_KERNEL) | defined(NNX_HAL)
    NTHALAPI
    VOID 
    NTAPI    
    PicInitialize();

    NTHALAPI
    VOID 
    NTAPI
    PicDisableForApic();

#endif

#ifdef __cplusplus
}
#endif

#endif