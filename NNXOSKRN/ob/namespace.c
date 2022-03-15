#include "object.h"

typedef struct _OBJECT_DIRECTORY_IMPL
{
    LIST_ENTRY ChildrenHead;
}OBJECT_DIRECTORY, *POBJECT_DIRECTORY;

HANDLE GlobalNamespace = INVALID_HANDLE_VALUE;


NTSTATUS ObpInitNamespace()
{
    NTSTATUS status;

    status = STATUS_SUCCESS;

    

    return status;
}

HANDLE ObGetGlobalNamespaceHandle()
{
    return GlobalNamespace;
}