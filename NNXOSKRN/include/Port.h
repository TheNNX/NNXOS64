#ifndef NNX_PORT_HEADER
#define NNX_PORT_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

    NTHALAPI VOID NTAPI outb(UINT16, UINT8);
    NTHALAPI VOID NTAPI outw(UINT16, UINT16);
    NTHALAPI VOID NTAPI outd(UINT16, UINT32);
    NTHALAPI UINT8 NTAPI inb(UINT16);
    NTHALAPI UINT16 NTAPI inw(UINT16);
    NTHALAPI UINT32 NTAPI ind(UINT16);

#ifdef __cplusplus
}
#endif

#endif