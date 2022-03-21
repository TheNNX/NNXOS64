#ifndef NNX_HANDLE_HEADER
#define NNX_HANDLE_HEADER

#include <nnxtype.h>
#include <ntlist.h>
#include <HAL/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef PVOID HANDLE, *PHANDLE;

    typedef struct _HANDLE_DATABASE_ENTRY
    {
        /* for enumeration, list head in the object header */
        LIST_ENTRY ObjectHandleEntry;
        PVOID Object;
    }HANDLE_DATABASE_ENTRY, *PHANDLE_DATABASE_ENTRY;

#define ENTRIES_PER_HANDLE_DATABASE (PAGE_SIZE - sizeof(LIST_ENTRY)) / sizeof(HANDLE_DATABASE_ENTRY)

    typedef __declspec(align(4096)) struct _HANDLE_DATABASE
    {
        /* entry in the process' handle database chain (it has to be the first element of the struct) */
        LIST_ENTRY HandleDatabaseChainEntry;

        HANDLE_DATABASE_ENTRY Entries[ENTRIES_PER_HANDLE_DATABASE];
    }HANDLE_DATABASE, *PHANDLE_DATABASE;

#define INVALID_HANDLE_VALUE (HANDLE)((ULONG_PTR)-1)

    VOID ObCloseHandleByEntry(PHANDLE_DATABASE_ENTRY entry);
    NTSTATUS ObCloseHandle(HANDLE handle, KPROCESSOR_MODE accessMode);
    NTSTATUS ObInitHandleManager();
    NTSTATUS ObExtractAndReferenceObjectFromHandle(HANDLE handle, PVOID *pObject, KPROCESSOR_MODE accessMode);
    NTSTATUS ObCreateHandle(PHANDLE pOutHandle, KPROCESSOR_MODE accessMode, PVOID object);
#ifdef __cplusplus
}
#endif

#endif