#ifndef NNX_NAMESPACE_HEADER_PUB
#define NNX_NAMESPACE_HEADER_PUB

#include <nnxtype.h>

#ifdef __cplusplus
extern "C"
{
#endif

    NTSTATUS ObpInitNamespace();
    HANDLE ObGetGlobalNamespaceHandle();

    extern HANDLE GlobalNamespace;

#ifdef __cplusplus
}
#endif
#endif

/* excplicitly define to include */
#ifdef NNX_NAMESPACE_HEADER_PRIV
#undef NNX_NAMESPACE_HEADER_PRIV

typedef struct _OBJECT_GLOBAL_ROOT OBJECT_GLOBAL_ROOT, *POBJECT_GLOBAL_ROOT;

#endif