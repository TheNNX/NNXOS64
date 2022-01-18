#ifndef NNX_PORT_HEADER
#define NNX_PORT_HEADER

#include <nnxtype.h>

#ifdef __cplusplus
extern "C" {
#endif

    VOID outb(UINT16, UINT8);
    VOID outw(UINT16, UINT16);
    VOID outd(UINT16, UINT32);
    UINT8 inb(UINT16);
    UINT16 inw(UINT16);
    UINT32 ind(UINT16);

#ifdef __cplusplus
}
#endif

#endif