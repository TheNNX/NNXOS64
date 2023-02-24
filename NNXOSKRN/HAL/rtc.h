#ifndef NNX_RTC_HEADER
#define NNX_RTC_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

    VOID HalRtcInitialize(UCHAR CenturyRegister);
    
    UCHAR HalRtcGetSeconds();
    UCHAR HalRtcGetMinutes();
    UCHAR HalRtcGetHours();
    UCHAR HalRtcGetDay();
    UCHAR HalRtcGetMonth();
    USHORT HalRtcGetYear();

    VOID HalpPrintCurrentTime();
    VOID HalpPrintCurrentDate();
    
    VOID 
    NTAPI 
    KeQuerySystemTime(PULONG64 CurrentTime);

#ifdef __cplusplus
}
#endif
#endif