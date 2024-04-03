#ifndef NNX_PRELOADED_HEADER
#define NNX_PRELOADED_HEADER

#include <ntlist.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern PLIST_ENTRY KePreloadedHeadPtr;

    NTSTATUS
    NTAPI
    KeInitPreloadedFilesystem();

#ifdef __cplusplus
}
#endif

#endif