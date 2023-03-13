#ifndef NNX_RTC_HEADER
#define NNX_RTC_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    NTSYSAPI
    VOID 
    NTAPI 
    KeQuerySystemTime(PULONG64 CurrentTime);

#ifdef NNX_KERNEL
    VOID 
    NTAPI
    HalRtcInitialize(UCHAR CenturyRegister);

    UCHAR 
    NTAPI
    HalRtcGetSeconds();

    UCHAR 
    NTAPI
    HalRtcGetMinutes();

    UCHAR 
    NTAPI
    HalRtcGetHours();
    
    UCHAR 
    NTAPI
    HalRtcGetDay();
    
    UCHAR 
    NTAPI
    HalRtcGetMonth();
    
    USHORT 
    NTAPI
    HalRtcGetYear();

    VOID 
    NTAPI
    HalpPrintCurrentTime();
    
    VOID 
    NTAPI
    HalpPrintCurrentDate();
#endif

#ifdef __cplusplus
}
#endif
#endif