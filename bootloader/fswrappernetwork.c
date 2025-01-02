#include "fswrapper.h"
#include <efilib.h>
#include "../NNXOSKRN/nnxver.h"
#include "stringa.h"

#ifdef NET_DEBUG
#define TRACE(...) do{Print(L"[" __FILE__ ":%d] ", __LINE__); Print(__VA_ARGS__);}while(0)
#else
#define TRACE(...) do{}while(0)
#endif

EFI_GUID gEfiTcp4ServiceBindingProtocolGuid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
EFI_GUID gEfiTcp4ProtocolGuid = EFI_TCP4_PROTOCOL;

typedef struct _FSWRAPPER_NETWORK_CTX
{
    EFI_SERVICE_BINDING* TcpServiceBinding;
    CHAR16* Url;
    CHAR16* Filename;
    UINT64 FilePosition;

    FILE_WRAPPER_HANDLE Parent;
} FSWRAPPER_NETWORK_CTX, *PFSWRAPPER_NETWORK_CTX;

static
EFI_STATUS
DoRequest(
    EFI_SERVICE_BINDING* tcpServiceBinding,
    const CHAR16* url,
    OUT OPTIONAL CHAR8* buffer,
    IN OUT OPTIONAL size_t* pBufferSize,
    OUT OPTIONAL size_t* pTotalSize,
    size_t offset,
    INTN maxWaitSec);

static
EFI_STATUS
FsWrapperNetworkClose(
    FILE_WRAPPER_HANDLE Handle);

static
EFI_STATUS
FsWrapperNetworkOpen(
    FILE_WRAPPER_HANDLE self,
    FILE_WRAPPER_HANDLE* newHandle,
    CHAR16* filename,
    UINT64 openMode,
    UINT64 attributes);

static
EFI_STATUS
FsWrapperNetworkRead(
    FILE_WRAPPER_HANDLE self,
    UINTN* bufferSize,
    void* buffer);

static
EFI_STATUS
FsWrapperNetworkSetPosition(
    FILE_WRAPPER_HANDLE self,
    UINT64 position);

static
EFI_STATUS
FsWrapperNetworkGetInfo(
    FILE_WRAPPER_HANDLE self,
    EFI_GUID* infoType,
    UINTN* bufferSize,
    void* buffer);

/* Root network FILE_WRAPPERs do not support Read, SetPosition, GetInfo etc. 
   Functions below are needed for them to return appropriate status code instead
   of haning on NULL dereference. */
static
EFI_STATUS
FsWrapperNetworkReadUnsupported(
    FILE_WRAPPER_HANDLE self,
    UINTN* bufferSize,
    void* buffer)
{
    return EFI_UNSUPPORTED;
}

static
EFI_STATUS
FsWrapperNetworkSetPositionUnsupported(
    FILE_WRAPPER_HANDLE self,
    UINT64 position)
{
    return EFI_UNSUPPORTED;
}

static
EFI_STATUS
FsWrapperNetworkGetInfoUnsupported(
    FILE_WRAPPER_HANDLE self,
    EFI_GUID* infoType,
    UINTN* bufferSize,
    void* buffer)
{
    return EFI_UNSUPPORTED;
}

/* Helper functions for populating different types of network FILE_WRAPPER */
static
EFI_STATUS
FsWrapperNetworkPopulateCommon(
    FILE_WRAPPER_HANDLE dst,
    EFI_SERVICE_BINDING* tcpServiceBinding)
{
    EFI_STATUS status;
    PFSWRAPPER_NETWORK_CTX ctx;

    status = gBS->AllocatePool(EfiLoaderData, sizeof(FSWRAPPER_NETWORK_CTX), &dst->pWrapperContext);
    if (EFI_ERROR(status))
    {
        return status;
    }

    ZeroMem(dst->pWrapperContext, sizeof(FSWRAPPER_NETWORK_CTX));
    ctx = dst->pWrapperContext;
    ctx->TcpServiceBinding = tcpServiceBinding;

    dst->Close = FsWrapperNetworkClose;
    dst->Open = FsWrapperNetworkOpen;
    dst->Read = FsWrapperNetworkReadUnsupported;
    dst->SetPosition = FsWrapperNetworkSetPositionUnsupported;
    dst->GetInfo = FsWrapperNetworkGetInfoUnsupported;

    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperNetworkPopulateRoot(
    FILE_WRAPPER_HANDLE dst,
    const CHAR16* serverUrl,
    EFI_SERVICE_BINDING* binding)
{
    EFI_STATUS status;
    PFSWRAPPER_NETWORK_CTX ctx;

    CHAR16* filename, *url;

    filename = StrDuplicate(L"/");

    status = gBS->AllocatePool(EfiLoaderData, sizeof(*url) * (2 + StrLen(serverUrl)), &url);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(filename);
        return status;
    }

    StrCpy(url, serverUrl);
    StrCat(url, L"");

    status = FsWrapperNetworkPopulateCommon(dst, binding);
    if (EFI_ERROR(status))
    {
        return status;
    }

    ctx = dst->pWrapperContext;

    ctx->Url = url;
    ctx->Filename = filename;
    ctx->Parent = NULL;

    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperNetworkPopulateChild(
    FILE_WRAPPER_HANDLE dst,
    EFI_SERVICE_BINDING* binding,
    FILE_WRAPPER_HANDLE parent,
    const CHAR16* parentUrl,
    const CHAR16* filename)
{
    EFI_STATUS status;
    PFSWRAPPER_NETWORK_CTX ctx;
    size_t parentUrlLen, filenameLen;

    CHAR16* filenameBuffer, *urlBuffer;

    filenameLen = StrLen(filename);
    parentUrlLen = StrLen(parentUrl);

    if (parentUrl == NULL || filename == NULL || dst == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status = gBS->AllocatePool(
        EfiLoaderData, 
        (parentUrlLen + filenameLen) * sizeof(*urlBuffer) 
            + sizeof(L'/') + sizeof(L'\0'),
        &urlBuffer);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->AllocatePool(
        EfiLoaderData, 
        filenameLen * sizeof(*filenameBuffer), 
        &filenameBuffer);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(urlBuffer);
        return status;
    }

    StrCpy(urlBuffer, parentUrl);
    StrCat(urlBuffer, L"/");
    StrCat(urlBuffer, filename);

    StrCpy(filenameBuffer, filename);

    status = FsWrapperNetworkPopulateCommon(dst, binding);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(urlBuffer);
        gBS->FreePool(filenameBuffer);
        return status;
    }

    dst->Read = FsWrapperNetworkRead;
    dst->SetPosition = FsWrapperNetworkSetPosition;
    dst->GetInfo = FsWrapperNetworkGetInfo;

    ctx = dst->pWrapperContext;
    ctx->FilePosition = 0;
    ctx->Url = urlBuffer;
    ctx->Filename = filenameBuffer;
    ctx->Parent = parent;

    return EFI_SUCCESS;
}

/* Function implementations */
static
EFI_STATUS
FsWrapperNetworkCloseCtx(
    PFSWRAPPER_NETWORK_CTX ctx)
{
    if (ctx == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (ctx->Filename != NULL)
    {
        gBS->FreePool(ctx->Filename);
    }

    if (ctx->Url != NULL)
    {
        gBS->FreePool(ctx->Url);
    }

    return gBS->FreePool(ctx);
}

static
EFI_STATUS
FsWrapperNetworkClose(
    FILE_WRAPPER_HANDLE Handle)
{
    EFI_STATUS status;

    if (Handle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status = FsWrapperNetworkCloseCtx(Handle->pWrapperContext);
    if (EFI_ERROR(status))
    {
        return status;
    }

    return gBS->FreePool(Handle);
}

static
EFI_STATUS
FsWrapperNetworkOpen(
    FILE_WRAPPER_HANDLE self,
    FILE_WRAPPER_HANDLE* pOutNewHandle,
    CHAR16* filename,
    UINT64 openMode,
    UINT64 attributes)
{
    EFI_STATUS status;
    FILE_WRAPPER_HANDLE newHandle;
    PFSWRAPPER_NETWORK_CTX selfCtx;

    if (self == NULL || pOutNewHandle == NULL || filename == NULL)
    {
        TRACE(L"%X, %X, %X", self, pOutNewHandle, filename);
        return EFI_INVALID_PARAMETER;
    }

    selfCtx = self->pWrapperContext;

    if (selfCtx == NULL || selfCtx->Url == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (StrCmp(filename, L"..") == 0)
    {
        /* There is no such thing as a parent for the root file. */
        if (selfCtx->Parent == NULL)
        {
            return EFI_NOT_FOUND;
        }
        return selfCtx->Parent->Open(selfCtx->Parent, pOutNewHandle, L".", openMode, attributes);
    }
    else if (StrCmp(filename, L".") == 0)
    {
        /* This is the root file handle, create a copy of it. */
        if (selfCtx->Parent == NULL)
        {
            PFSWRAPPER_NETWORK_CTX ctxClone;

            status = gBS->AllocatePool(EfiLoaderData, sizeof(*selfCtx), &ctxClone);
            if (EFI_ERROR(status))
            {
                return status;
            }

            status = gBS->AllocatePool(EfiLoaderData, sizeof(*newHandle), &newHandle);
            if (EFI_ERROR(status))
            {
                gBS->FreePool(ctxClone);
                return status;
            }

            CopyMem(ctxClone, selfCtx, sizeof(*ctxClone));
            CopyMem(newHandle, self, sizeof(*self));

            ctxClone->Filename = StrDuplicate(selfCtx->Filename);
            ctxClone->Url = StrDuplicate(selfCtx->Url);

            if (ctxClone->Filename == NULL || ctxClone->Url == NULL)
            {
                if (ctxClone->Filename != NULL)
                {
                    gBS->FreePool(ctxClone->Filename);
                }
                if (ctxClone->Url != NULL)
                {
                    gBS->FreePool(ctxClone->Url);
                }

                gBS->FreePool(ctxClone);
                gBS->FreePool(newHandle);

                return EFI_OUT_OF_RESOURCES;
            }

            newHandle->pWrapperContext = ctxClone;
            *pOutNewHandle = newHandle;
            return EFI_SUCCESS;
        }
        return selfCtx->Parent->Open(selfCtx->Parent, pOutNewHandle, selfCtx->Filename, openMode, attributes);
    }

    status = gBS->AllocatePool(EfiLoaderData, sizeof(*newHandle), &newHandle);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = FsWrapperNetworkPopulateChild(newHandle, selfCtx->TcpServiceBinding, self, selfCtx->Url, filename);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(newHandle);
        return status;
    }

    *pOutNewHandle = newHandle;
    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperNetworkRead(
    FILE_WRAPPER_HANDLE self,
    UINTN* bufferSize,
    void* buffer)
{
    PFSWRAPPER_NETWORK_CTX ctx;
    EFI_STATUS status;

    if (self == NULL || buffer == NULL || bufferSize == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    ctx = self->pWrapperContext;

    status = DoRequest(ctx->TcpServiceBinding, ctx->Url, buffer, bufferSize, NULL, ctx->FilePosition, 0);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error doing request - %r\n", status);
        return status;
    }

    ctx->FilePosition += *bufferSize;
    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperNetworkSetPosition(
    FILE_WRAPPER_HANDLE self,
    UINT64 position)
{
    PFSWRAPPER_NETWORK_CTX ctx;

    if (self == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    ctx = self->pWrapperContext;
    ctx->FilePosition = position;
    return EFI_SUCCESS;
}

static
EFI_STATUS
FsWrapperNetworkGetInfo(
    FILE_WRAPPER_HANDLE self,
    EFI_GUID* infoType,
    UINTN* bufferSize,
    void* buffer)
{
    EFI_FILE_INFO info = { 0 };
    size_t fileSize;
    size_t filenameLen;
    CHAR16* pFilenameInBuffer;
    PFSWRAPPER_NETWORK_CTX ctx;
    EFI_STATUS status;

    if (self == NULL || buffer == NULL || infoType == NULL || bufferSize == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    ctx = self->pWrapperContext;

    if (CompareGuid(infoType, &gEfiFileInfoGuid) != 0)
    {
        return EFI_UNSUPPORTED;
    }

    /* TODO: attributes, filetimes etc. */
    info.Size = sizeof(info);

    /* TODO: optimize by doing a HEAD request or a GET request with zero range. */
    status = DoRequest(ctx->TcpServiceBinding, ctx->Url, NULL, NULL, &fileSize, 0, 5);
    if (EFI_ERROR(status))
    {
        return status;
    }
    info.FileSize = fileSize;

    filenameLen = StrLen(ctx->Filename);
    if (*bufferSize < sizeof(info) + sizeof(CHAR16) * (filenameLen + 1))
    {
        TRACE(L"" __FUNCTION__ ": buffer too small (%X < %X + 2 * %X)\n", *bufferSize, sizeof(info), (filenameLen + 1));
        *bufferSize = sizeof(info) + sizeof(CHAR16) * (filenameLen + 1);
        return EFI_BUFFER_TOO_SMALL;
    }

    pFilenameInBuffer = (CHAR16*)(((UINT8*)buffer) + sizeof(info));
    CopyMem(buffer, &info, sizeof(info));
    CopyMem(pFilenameInBuffer, ctx->Filename, filenameLen * sizeof(CHAR16));
    pFilenameInBuffer[filenameLen] = L'\0';
    *bufferSize = sizeof(info) + sizeof(CHAR16) * (filenameLen + 1);

    return EFI_SUCCESS;
    
}

static
VOID
SetContextToTrue(
    EFI_EVENT Event,
    BOOLEAN* Context)
{
    *Context = TRUE;
}

static
EFI_STATUS
TcpConnectHelper(
    EFI_TCP4* tcp)
{
    size_t completed = FALSE;
    EFI_TCP4_CONNECTION_TOKEN token;
    EFI_STATUS status;

    token.CompletionToken.Status = EFI_NOT_READY;
    status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, SetContextToTrue, &completed, &token.CompletionToken.Event);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error %r\n", status);
        return status;
    }

    status = tcp->Connect(tcp, &token);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error %r\n", status);
        return status;
    }

    /* TODO: timeout */
    while (completed == FALSE)
    {
        tcp->Poll(tcp);
    }

    if (completed == FALSE)
    {
        tcp->Cancel(tcp, &token.CompletionToken);
        return EFI_TIMEOUT;
    }

    if (EFI_ERROR(token.CompletionToken.Status))
    {
        return token.CompletionToken.Status;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
TcpConnect(
    IN EFI_SERVICE_BINDING* tcpServiceBinding,
    OUT EFI_HANDLE* pTcpHandle,
    UINT32 ipv4Address,
    UINT16 port,
    UINTN maxDhcpRetries)
{
    EFI_STATUS status;
    EFI_TCP4_CONFIG_DATA tcpConfigData;
    EFI_HANDLE tcpHandle = NULL;
    EFI_TCP4* tcp;
    
    if (tcpServiceBinding == NULL || pTcpHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status = tcpServiceBinding->CreateChild(tcpServiceBinding, &tcpHandle);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->HandleProtocol(tcpHandle, &gEfiTcp4ProtocolGuid, &tcp);
    if (EFI_ERROR(status))
    {
        tcpServiceBinding->DestroyChild(tcpServiceBinding, tcpHandle);
        return status;
    }

    tcpConfigData.TypeOfService = 0;
    tcpConfigData.ControlOption = NULL;
    tcpConfigData.TimeToLive = 254;
    tcpConfigData.AccessPoint.UseDefaultAddress = TRUE;
    ZeroMem(&tcpConfigData.AccessPoint.StationAddress, sizeof(tcpConfigData.AccessPoint.StationAddress));
    ZeroMem(&tcpConfigData.AccessPoint.SubnetMask, sizeof(tcpConfigData.AccessPoint.SubnetMask));
    tcpConfigData.AccessPoint.StationPort = 0;
    tcpConfigData.AccessPoint.RemoteAddress.Addr[3] = ipv4Address & 0xFF;
    tcpConfigData.AccessPoint.RemoteAddress.Addr[2] = (ipv4Address >> 8) & 0xFF;
    tcpConfigData.AccessPoint.RemoteAddress.Addr[1] = (ipv4Address >> 16) & 0xFF;
    tcpConfigData.AccessPoint.RemoteAddress.Addr[0] = (ipv4Address >> 24) & 0xFF;
    tcpConfigData.AccessPoint.RemotePort = port;
    tcpConfigData.AccessPoint.ActiveFlag = TRUE;

    do
    {
        status = tcp->Configure(tcp, &tcpConfigData);
        if (status == EFI_NO_MAPPING)
        {
            TRACE(L"No mapping, DHCP retries left %d\n", maxDhcpRetries);
            gBS->Stall(1000000);

            if (maxDhcpRetries-- == 0)
            {
                break;
            }
        }
        else if (EFI_ERROR(status))
        {
            tcpServiceBinding->DestroyChild(tcpServiceBinding, tcpHandle);
            return status;
        }
    } while (status == EFI_NO_MAPPING);

    if (EFI_ERROR(status))
    {
        tcpServiceBinding->DestroyChild(tcpServiceBinding, tcpHandle);
        return status;
    }
    
    status = TcpConnectHelper(tcp);
    if (EFI_ERROR(status))
    {
        tcpServiceBinding->DestroyChild(tcpServiceBinding, tcpHandle);
        return status;
    }

    *pTcpHandle = tcpHandle;
    return EFI_SUCCESS;
}

static
EFI_STATUS
TcpClose(
    EFI_SERVICE_BINDING* tcpServiceBinding,
    EFI_HANDLE tcp)
{
    return tcpServiceBinding->DestroyChild(tcpServiceBinding, tcp);
}

static
EFI_STATUS
TcpReceivePartial(
    EFI_HANDLE tcpHandle,
    OUT void* buffer,
    IN OUT size_t* len)
{
    EFI_TCP4* tcp = NULL;
    EFI_STATUS status;
    BOOLEAN receiveOver = FALSE;
    EFI_TCP4_IO_TOKEN token = { 0 };
    EFI_TCP4_RECEIVE_DATA rxData;

    if (len == NULL || buffer == NULL || tcpHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status = gBS->HandleProtocol(tcpHandle, &gEfiTcp4ProtocolGuid, &tcp);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }

    rxData.DataLength = (UINT32)*len;
    rxData.FragmentCount = 1;
    rxData.FragmentTable[0].FragmentBuffer = buffer;
    rxData.FragmentTable[0].FragmentLength = (UINT32)*len;
    rxData.UrgentFlag = FALSE;
    token.Packet.RxData = &rxData;

    token.CompletionToken.Status = EFI_NOT_READY;
    status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL,
        TPL_CALLBACK,
        SetContextToTrue,
        &receiveOver,
        &token.CompletionToken.Event);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }

    status = tcp->Receive(tcp, &token);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }

    /* TODO: timeouts */
    while (receiveOver == FALSE)
    {
        tcp->Poll(tcp);
    }

    *len = rxData.DataLength;
    
    if (EFI_ERROR(token.CompletionToken.Status))
    {
        TRACE(L"Error - %r", token.CompletionToken.Status);
        return token.CompletionToken.Status;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
TcpSendPartial(
    EFI_HANDLE tcpHandle,
    IN const void* buffer,
    IN OUT size_t* len)
{
    EFI_TCP4* tcp = NULL;
    EFI_STATUS status;
    BOOLEAN transmissionOver = FALSE;
    EFI_TCP4_IO_TOKEN token = { 0 };
    EFI_TCP4_TRANSMIT_DATA txData;

    if (len == NULL || buffer == NULL || tcpHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status = gBS->HandleProtocol(tcpHandle, &gEfiTcp4ProtocolGuid, &tcp);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }

    txData.Push = FALSE;
    txData.Urgent = FALSE;
    txData.DataLength = (UINT32)*len;
    txData.FragmentCount = 1;
    txData.FragmentTable[0].FragmentLength = (UINT32)*len;
    txData.FragmentTable[0].FragmentBuffer = (void*)buffer;
    token.Packet.TxData = &txData;

    token.CompletionToken.Status = EFI_NOT_READY;
    status = gBS->CreateEvent(
        EVT_NOTIFY_SIGNAL, 
        TPL_CALLBACK, 
        SetContextToTrue, 
        &transmissionOver, 
        &token.CompletionToken.Event);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }

    status = tcp->Transmit(tcp, &token);
    if (EFI_ERROR(status))
    {
        TRACE(L"Error - %r", status);
        return status;
    }
   
    /* TODO: timeouts */
    while (transmissionOver == FALSE)
    {
        tcp->Poll(tcp);
    }

    *len = txData.DataLength;

    if (EFI_ERROR(token.CompletionToken.Status))
    {
        TRACE(L"Error - %r", token.CompletionToken.Status);
        return token.CompletionToken.Status;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
TcpReceive(
    EFI_HANDLE tcpHandle,
    OUT void* buffer,
    size_t len)
{
    EFI_STATUS status;

    while (len > 0)
    { 
        size_t curLen = len;

        status = TcpReceivePartial(tcpHandle, buffer, &curLen);
        if (EFI_ERROR(status))
        {
            return status;
        }

        buffer = ((char*)buffer) + curLen;
        len -= curLen;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
TcpSend(
    EFI_HANDLE tcpHandle,
    IN const void* buffer,
    size_t len)
{
    EFI_STATUS status;

    while (len > 0)
    {
        size_t curLen = len;

        status = TcpSendPartial(tcpHandle, buffer, &curLen);
        if (EFI_ERROR(status))
        {
            return status;
        }

        buffer = ((char*)buffer) + curLen;
        len -= curLen;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
EnsureSpaceForCharacter(
    size_t* pLineLen,
    size_t* pLineBufLen,
    CHAR8** pLineBuf)
{
    EFI_STATUS status;
    size_t oldBufLen = 0;
    CHAR8* lineBufCopy = NULL;

    if (*pLineBufLen <= *pLineLen)
    {
        oldBufLen = *pLineBufLen;
        *pLineBufLen = (1 + *pLineBufLen) * 2;
        if (*pLineBuf == NULL)
        {
            status = gBS->AllocatePool(EfiLoaderData, *pLineBufLen, pLineBuf);
            if (EFI_ERROR(status))
            {
                return status;
            }
        }
        else
        {
            status = gBS->AllocatePool(EfiLoaderData, *pLineBufLen, &lineBufCopy);
            if (EFI_ERROR(status))
            {
                gBS->FreePool(*pLineBuf);
                return status;
            }
            CopyMem(lineBufCopy, *pLineBuf, oldBufLen);
            gBS->FreePool(*pLineBuf);
            *pLineBuf = lineBufCopy;
            lineBufCopy = NULL;
        }
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
GetHttpLine(
    EFI_HANDLE hTcp,
    OUT CHAR8** pLine,
    OUT size_t* pLineLen)
{
    EFI_STATUS status;
    EFI_TCP4* tcp;

    size_t lineLen = 0;
    size_t lineBufLen = 0;
    CHAR8* lineBuf = NULL;

    if (hTcp == NULL)
    {
        return EFI_INVALID_LANGUAGE;
    }

    status = gBS->HandleProtocol(hTcp, &gEfiTcp4ProtocolGuid, &tcp);
    if (EFI_ERROR(status))
    {
        return status;
    }

    while (lineLen < 2 || 
           lineBuf[lineLen - 2] != '\r' || 
           lineBuf[lineLen - 1] != '\n')
    {
        char tmpBuf[1];

        status = EnsureSpaceForCharacter(&lineLen, &lineBufLen, &lineBuf);
        if (EFI_ERROR(status))
        {
            gBS->FreePool(lineBuf);
            return status;
        }

        status = TcpReceive(hTcp, tmpBuf, sizeof(tmpBuf));
        if (EFI_ERROR(status))
        {
            gBS->FreePool(lineBuf);
            return status;
        }

        lineBuf[lineLen++] = tmpBuf[0];
    }
    
    lineBuf[--lineLen] = '\0';
    lineBuf[--lineLen] = '\0';

    *pLine = lineBuf;
    *pLineLen = lineLen;
    return EFI_SUCCESS;
}

static
char
GetLowercase(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A' + 'a';
    }
    return c;
}

static
BOOLEAN
IsWhitespace(char c)
{
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
        return TRUE;
    }

    return FALSE;
}

static
EFI_STATUS
DoRequest(
    EFI_SERVICE_BINDING* tcpServiceBinding,
    const CHAR16* url,
    OUT OPTIONAL CHAR8* buffer,
    IN OUT OPTIONAL size_t* pBufferSize,
    OUT OPTIONAL size_t* pTotalSize,
    size_t offset,
    INTN maxWaitSec)
{
    EFI_HANDLE tcpHandle;
    EFI_STATUS status;
    /* TODO: hardcoded values bad */
    CHAR8 *requestBuffer;
    CHAR8 responseBuffer[512] = { 0 };
    CHAR8 urlBuf[256];
    size_t len, i, j, curBufPos, curTotPos;
    size_t numChunks, sizeChunk, curSize;
    UINT32 ipResult;
    const CHAR16* urlExpectedStart = L"http://";
    const CHAR8* contentLength = "content-length:";
    
    /* TODO: ranges */
    const CHAR8* rangesAccepted = "HTTP/1.1 205";
    const CHAR8* rangesIgnored = "HTTP/1.1 200";
    const CHAR8* rangesInvalid = "HTTP/1.1 416";
    const CHAR8* fileNotFound = "HTTP/1.1 404";
    
    UINT32 currentIpSegment = 0;
    size_t lineLen;
    CHAR8* line;
    size_t totSize = 0;

    status = gBS->AllocatePool(EfiLoaderData, 10000, &requestBuffer);
    if (EFI_ERROR(status))
    {
        return status;
    }

    for (i = 0; i < StrLen(urlExpectedStart); i++)
    {
        if (urlExpectedStart[i] != url[i])
        {
            TRACE(L"Unsupported protocol specifiedd in URL: %s\n", url);
            gBS->FreePool(requestBuffer);
            return EFI_UNSUPPORTED;
        }
    }

    url += StrLen(urlExpectedStart);
    j = 0;
    ipResult = 0;

    while (*url && *url != L'/' && *url != L':')
    {
        if (*url == '.')
        {
            ipResult <<= 8;
            ipResult |= currentIpSegment;
            currentIpSegment = 0;
            j = 0;
        }
        else if (j >= 3 || *url < L'0' || *url > L'9')
        {
            TRACE(L"Invalid IP address\n");
            gBS->FreePool(requestBuffer);
            return EFI_INVALID_PARAMETER;
        }
        else
        {
            currentIpSegment *= 10;
            currentIpSegment += (*url - '0');
            j++;
        }

        url++;
    }

    ipResult <<= 8;
    ipResult |= currentIpSegment;
    currentIpSegment = 0;
    j = 0;

    if (*url == ':')
    {
        /* TODO: handle port number */
        return EFI_UNSUPPORTED;
    }

    status = TcpConnect(tcpServiceBinding, &tcpHandle, ipResult, 80, 10);
    if (EFI_ERROR(status))
    {
        TRACE(L"Starting TCP connection failed - %r\n", status);
        gBS->FreePool(requestBuffer);
        return status;
    }

    StrWToStrA(urlBuf, url);

    StrCpyA(requestBuffer, "GET ");
    StrCatA(requestBuffer, urlBuf);
    StrCatA(requestBuffer, " HTTP/1.1\r\n");
    StrCatA(requestBuffer, "User-Agent: " NNX_OSNAME "/" NNX_VER_STR "\r\n");

    StrCatA(requestBuffer, "\r\n");

    len = StrLenA(requestBuffer);
    status = TcpSend(tcpHandle, requestBuffer, len);
    gBS->FreePool(requestBuffer);
    if (EFI_ERROR(status))
    {
        TcpClose(tcpServiceBinding, tcpHandle);
        return status;
    }

    /* Get the status line. */
    status = GetHttpLine(tcpHandle, &line, &lineLen);
    if (EFI_ERROR(status))
    {
        TcpClose(tcpServiceBinding, tcpHandle);
        return status;
    }
    /* TODO: verify validity, get the status code, etc. */
    gBS->FreePool(line);

    /* Get all headers. */
    status = GetHttpLine(tcpHandle, &line, &lineLen);
    if (EFI_ERROR(status))
    {
        TcpClose(tcpServiceBinding, tcpHandle);
        return status;
    }
    while (StrCmpA(line, "") != 0)
    {
        const CHAR8* curPos = line;
        BOOLEAN found = TRUE;

        for (i = 0; i < StrLenA(contentLength) && found; i++)
        {
            if (GetLowercase(*curPos++) != GetLowercase(contentLength[i]))
            {
                found = FALSE;
                break;
            }
        }

        if (found == TRUE)
        {
            const CHAR8* valPos;

            valPos = line + StrLenA(contentLength);
            while (IsWhitespace(*valPos))
            {
                valPos++;
            }

            while (*valPos)
            {
                totSize = 10 * totSize + (*valPos++) - '0';
            }
        }

        gBS->FreePool(line);

        status = GetHttpLine(tcpHandle, &line, &lineLen);
        if (EFI_ERROR(status))
        {
            TcpClose(tcpServiceBinding, tcpHandle);
            return status;
        }
    }
    gBS->FreePool(line);

    if (buffer == NULL)
    {
        if (pTotalSize != NULL)
        {
            *pTotalSize = totSize;
        }

        TcpClose(tcpServiceBinding, tcpHandle);
        return EFI_SUCCESS;
    }

    sizeChunk = sizeof(responseBuffer);
    numChunks = (totSize + sizeChunk - 1) / sizeChunk;
    curBufPos = 0;
    curTotPos = 0;

    for (i = 0; i < numChunks; i++)
    {
        if (buffer == NULL)
        {
            break;
        }

        curSize = sizeChunk;
        if (i == numChunks - 1)
        {
            curSize = totSize % sizeChunk;
            if (curSize == 0)
            {
                curSize = sizeChunk;
            }
        }

        status = TcpReceive(tcpHandle, responseBuffer, curSize);
        if (EFI_ERROR(status))
        {
            TcpClose(tcpServiceBinding, tcpHandle);
            return status;
        }

        for (j = 0; j < curSize; j++)
        {
            if (buffer != NULL && pBufferSize != NULL)
            {
                if (curTotPos >= offset)
                {
                    if (curBufPos >= *pBufferSize)
                    {
                        buffer = NULL;
                        break;
                    }

                    buffer[curBufPos++] = responseBuffer[j];
                }
            }
            curTotPos++;
        }
    }

    if (pBufferSize != NULL)
    {
        *pBufferSize = curBufPos;
    }
    
    if (pTotalSize != NULL)
    {
        *pTotalSize = totSize;
    }

    TcpClose(tcpServiceBinding, tcpHandle);
    return status;
}

static
EFI_STATUS
TryInitProtocol(EFI_HANDLE protocolHandle)
{
    EFI_STATUS status;
    EFI_SIMPLE_NETWORK* protocol = NULL;
    EFI_SERVICE_BINDING* tcpBinding = NULL;
    BOOLEAN wasStarted = FALSE;

    status = gBS->HandleProtocol(protocolHandle, &gEfiSimpleNetworkProtocolGuid, &protocol);
    if (EFI_ERROR(status))
    {
        TRACE(L"%r\n", status);
        return status;
    }

    /* Check if this handle supports TCP */
    status = gBS->HandleProtocol(protocolHandle, &gEfiTcp4ServiceBindingProtocolGuid, &tcpBinding);
    if (EFI_ERROR(status))
    {
        TRACE(L"%r\n", status);
        return status;
    }

    status = protocol->Start(protocol);
    if (status == EFI_ALREADY_STARTED)
    {
        wasStarted = TRUE;
        TRACE(L"Already started.");
    }
    else if (EFI_ERROR(status))
    {
        TRACE(L"%r\n", status);
        return status;
    }

    status = protocol->Initialize(protocol, 0, 0);
    if (EFI_ERROR(status))
    {
        /* If the protocol was started before this function was called,
           do not close it. */
        if (wasStarted == FALSE)
        {
            //protocol->Stop(protocol);
        }
        TRACE(L"Warn - %r\n", status);
        //return status;
    }

    return EFI_SUCCESS;
}

static
EFI_STATUS
FindNetworkProtocol(
    OUT EFI_HANDLE* pProtocolHandle)
{
    EFI_STATUS status;
    EFI_HANDLE protocolHandle = NULL;
    EFI_HANDLE* buffer = NULL;
    UINTN numHandles = 0, i;

    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* toText = NULL;
    EFI_DEVICE_PATH_PROTOCOL* devicePath = NULL;

    if (pProtocolHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    status =
        gBS->LocateHandleBuffer(
            ByProtocol,
            &gEfiTcp4ServiceBindingProtocolGuid,
            NULL,
            &numHandles,
            &buffer);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, &toText);
    if (EFI_ERROR(status))
    {
        TRACE(L"Couldn't get conversion protocol\n");
    }

    for (i = 0; i < numHandles; i++)
    {
        Print(L"\n");
        TRACE(L"Trying protocol %d\n", i);

        status = gBS->HandleProtocol(buffer[i], &gEfiDevicePathProtocolGuid, &devicePath);
        TRACE(L"Getting path - %r\n", status);
        TRACE(L"Device %s\n", toText->ConvertDevicePathToText(devicePath, TRUE, TRUE));

        status = TryInitProtocol(buffer[i]);
        if (EFI_ERROR(status))
        {
            TRACE(L"Protocol init failed - %r\n", status);
            continue;
        }

        protocolHandle = buffer[i];
    }

    gBS->FreePool(buffer);

    if (protocolHandle != NULL)
    {
        *pProtocolHandle = protocolHandle;
        return EFI_SUCCESS;
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS 
FsWrapperOpenNetworkRoot(
    FILE_WRAPPER_HANDLE* pOut,
    const CHAR16* address)
{
    FILE_WRAPPER_HANDLE wrapperHandle;
    EFI_HANDLE protocolHandle;
    EFI_STATUS status;
    EFI_SERVICE_BINDING* serviceBinding = NULL;

    status = gBS->AllocatePool(EfiLoaderData, sizeof(FILE_WRAPPER_HANDLE), &wrapperHandle);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = FindNetworkProtocol(&protocolHandle);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(wrapperHandle);
        return status;
    }

    status = gBS->HandleProtocol(protocolHandle, &gEfiTcp4ServiceBindingProtocolGuid, &serviceBinding);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(wrapperHandle);
        return status;
    }

    TRACE(L"Service binding %X\n", serviceBinding);
    status = FsWrapperNetworkPopulateRoot(wrapperHandle, address, serviceBinding);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(wrapperHandle);
        return status;
    }

    *pOut = wrapperHandle;
    return EFI_SUCCESS;
}
