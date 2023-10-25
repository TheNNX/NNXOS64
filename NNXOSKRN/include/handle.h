#ifndef NNX_HANDLE_HEADER
#define NNX_HANDLE_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include <cpu.h>
#include <paging.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef PVOID HANDLE, *PHANDLE;

    typedef struct _HANDLE_DATABASE_ENTRY
    {
        /* for enumeration, list head in the object header */
        PVOID Object;
        ULONG_PTR Attributes;
    }HANDLE_DATABASE_ENTRY, *PHANDLE_DATABASE_ENTRY;

#define ENTRIES_PER_HANDLE_DATABASE ((PAGE_SIZE - sizeof(LIST_ENTRY)) / sizeof(HANDLE_DATABASE_ENTRY))

    typedef struct _HANDLE_DATABASE
    {
        /* entry in the process' handle database chain (it has to be the first element of the struct) */
        LIST_ENTRY HandleDatabaseChainEntry;

        HANDLE_DATABASE_ENTRY Entries[ENTRIES_PER_HANDLE_DATABASE];
    }HANDLE_DATABASE, *PHANDLE_DATABASE;

    NTSTATUS
    NTAPI
    ObCloseHandle(
        HANDLE handle,
        KPROCESSOR_MODE accessMode);

#ifdef NNX_KERNEL
    VOID 
    NTAPI
    ObCloseHandleByEntry(
        PHANDLE_DATABASE_ENTRY entry);
    
    NTSTATUS 
    NTAPI
    ObInitHandleManager();
    
    NTSTATUS 
    NTAPI
    ObExtractAndReferenceObjectFromHandle(
        HANDLE handle, 
        PVOID *pObject, 
        KPROCESSOR_MODE accessMode);
    
    NTSTATUS 
    NTAPI
    ObCreateHandle(
        PHANDLE pOutHandle, 
        KPROCESSOR_MODE accessMode, 
        PVOID object);

#endif

#ifdef __cplusplus
}
#endif

#endif