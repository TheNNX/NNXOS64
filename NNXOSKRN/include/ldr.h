#ifndef NNX_LDR_HEADER
#define NNX_LDR_HEADER

#include <nnxtype.h>
#include <object.h>
#include <handle.h>
#include <rtlstring.h>
#include <scheduler.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NNX_KERNEL

    NTSTATUS
    NTAPI
    LdrpInitialize();

    NTSTATUS
    NTAPI
    LdrpLoadImage(
        PCUNICODE_STRING SearchPath,
        PCUNICODE_STRING ImageName,
        PHANDLE pOutMoudleHandle);

    NTSTATUS
    NTAPI
    LdrpUnloadImage(
        HANDLE Module);

    typedef struct _LOAD_IMAGE_INFO
    {
        LIST_ENTRY DependenciesHead;
        LIST_ENTRY MemorySectionsHead;
    }LOAD_IMAGE_INFO, *PLOAD_IMAGE_INFO;

    typedef struct _KMODULE
    {
        PCUNICODE_STRING Name;
        HANDLE           File;
        ULONG_PTR        ImageBase;
        KSPIN_LOCK       Lock;
        LIST_ENTRY       InstanceHead;
        LIST_ENTRY       LoadedModulesEntry;
        LOAD_IMAGE_INFO  LoadImageInfo;
    }KMODULE, *PKMODULE;

    typedef struct _KLOADED_MODULE_INSTANCE
    {
        LIST_ENTRY  ProcessListEntry;
        LIST_ENTRY  ModuleListEntry;
        PKMODULE    Module;
        PEPROCESS   Process;
        ULONG_PTR   References;
    }KLOADED_MODULE_INSTANCE, *PKLOADED_MODULE_INSTANCE;
#endif

#ifdef __cplusplus
}
#endif

#endif