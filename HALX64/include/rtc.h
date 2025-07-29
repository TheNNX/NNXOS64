#ifndef NNX_RTC_HEADER
#define NNX_RTC_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#if defined(NNX_KERNEL) || defined(NNX_HAL)
    NTHALAPI
    VOID
    NTAPI
    HalRtcGetTime(PULONG64 outCurrentTime);

    NTHALAPI
    VOID 
    NTAPI
    HalRtcInitialize(UCHAR CenturyRegister);

    NTHALAPI
    UCHAR 
    NTAPI
    HalRtcGetSeconds();

    NTHALAPI
    UCHAR 
    NTAPI
    HalRtcGetMinutes();

    NTHALAPI
    UCHAR 
    NTAPI
    HalRtcGetHours();
    
    NTHALAPI
    UCHAR 
    NTAPI
    HalRtcGetDay();
    
    NTHALAPI
    UCHAR 
    NTAPI
    HalRtcGetMonth();
    
    NTHALAPI
    USHORT 
    NTAPI
    HalRtcGetYear();
#endif

#ifdef __cplusplus
}
#endif
#endif