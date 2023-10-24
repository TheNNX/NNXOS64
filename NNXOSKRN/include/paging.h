#ifndef NNX_PAGING_HEADER
#define NNX_PAGING_HEADER
#include <nnxtype.h>
#include <ntlist.h>

#ifdef _M_AMD64
#define PAGE_PRESENT 1
#define PAGE_WRITE 2
#define PAGE_USER 4
#define PAGE_WRITE_THROUGH 8
#define PAGE_NO_CACHE 16
#define PAGE_ACCESSED 32
#define PAGE_DIRTY 64
#define PAGE_GLOBAL 256

#define KERNEL_DESIRED_LOCATION 0xFFFFFF8000000000ULL
#define KERNEL_DESIRED_PML4_ENTRY (KERNEL_DESIRED_LOCATION >> (55ULL))

#define PAGING_KERNEL_SPACE      KERNEL_DESIRED_LOCATION
#define PAGING_KERNEL_SPACE_END  0xFFFFFFFFFFFFFFFFULL
#define PAGING_USER_SPACE        0x0000000000000000ULL
#define PAGING_USER_SPACE_END    0x00007FFFFFFFFFFFULL

#define PAGE_SYSTEM 0
#define PAGE_READ 0
#define PAGE_NOT_PRESENT 0

#define PAGE_SIZE 4096
#define PAGE_FLAGS_MASK 0xFFFULL
#define PAGE_ADDRESS_MASK (~PAGE_FLAGS_MASK)
#define PAGE_ALIGN(x) (((ULONG_PTR)x)&PAGE_ADDRESS_MASK)

typedef struct _ADDRESS_SPACE
{
    ULONG_PTR  TopStructPhysAddress;
    LIST_ENTRY SectionLinkHead;
    KSPIN_LOCK Lock;
}ADDRESS_SPACE, *PADDRESS_SPACE;

inline ULONG_PTR ToCanonicalAddress(ULONG_PTR address)
{
    return address | ((address & (1ULL << 47ULL)) ? (0xFFFF000000000000) : 0);
}

#elif

#error "Architecture unsupported"

#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NNX_KERNEL
    extern ULONG_PTR KeKernelPhysicalAddress;

    NTSTATUS
    PagingInit();

    VOID 
    PagingKernelInit();

    ULONG_PTR
    PagingAllocatePageWithPhysicalAddress(
        ULONG_PTR min, 
        ULONG_PTR max, 
        UINT16 flags,
        ULONG_PTR physPage);

    ULONG_PTR
    PagingAllocatePageEx(
        ULONG_PTR min, 
        ULONG_PTR max,
        UINT16 flags);

    ULONG_PTR
        PagingAllocatePage();

    ULONG_PTR
    PagingAllocatePageFromRange(
        ULONG_PTR min, 
        ULONG_PTR max);

    NTSTATUS
    PagingMapPage(
        ULONG_PTR v, 
        ULONG_PTR p, 
        UINT16 f);

    ULONG_PTR
    PagingAllocatePageBlockWithPhysicalAddresses(
        SIZE_T n, 
        ULONG_PTR min,
        ULONG_PTR max, 
        UINT16 flags,
        ULONG_PTR physFirstPage);

    ULONG_PTR
    PagingAllocatePageBlockFromRange(
        SIZE_T n, 
        ULONG_PTR min,
        ULONG_PTR max);

    ULONG_PTR 
    PagingAllocatePageBlock(
        SIZE_T n,
        UINT16 flags);

    ULONG_PTR
    PagingAllocatePageBlockEx(
        SIZE_T n, 
        ULONG_PTR min, 
        ULONG_PTR max, 
        UINT16 flags);

    VOID 
    PagingMapAndInitFramebuffer();

    ULONG_PTR 
    PagingMapStrcutureToVirtual(
        ULONG_PTR physicalAddress, 
        SIZE_T structureSize,
        UINT16 flags);

    ULONG_PTR 
    PagingFindFreePages(
        ULONG_PTR min, 
        ULONG_PTR max, 
        SIZE_T count);

    NTSTATUS
    NTAPI
    MmCreateAddressSpace(
        PADDRESS_SPACE pOutAddressSpace);

    VOID 
    NTAPI
    MmApplyAddressSpace(
        PADDRESS_SPACE AddressSpace);

    VOID
    NTAPI
    MmCopyCurrentAddressSpaceRef(
        PADDRESS_SPACE pOutAddressSpace);
    
    ULONG_PTR
    PagingGetTableMapping(
        ULONG_PTR virtualAddress);

    NTSTATUS
    PagingPageOutPage(
        ULONG_PTR virtualAddress);

    NTSTATUS
    PagingPageInPage(
        ULONG_PTR virtualAddress);

    NTSTATUS
    PagingInitializePageFile(
        SIZE_T pageFileSize,
        const char* filePath,
        struct VIRTUAL_FILE_SYSTEM* filesystem);

    VOID
    NTAPI
    PagingEnableSystemWriteProtection(
        VOID);

    VOID
    NTAPI
    PagingDisableSystemWriteProtection(
        VOID);

    NTHALAPI
    VOID
    NTAPI
    PagingInvalidatePage(
        ULONG_PTR);

    NTHALAPI
    VOID
    NTAPI
    PagingTLBFlush();

    VOID
    NTAPI
    PagingTLBFlushPage(
        ULONG_PTR page);

#endif

#ifdef __cplusplus
}
#endif

#endif