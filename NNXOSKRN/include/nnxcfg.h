#ifndef NNX_CONFIG_HEADER 
#define NNX_CONFIG_HEADER

#include <paging.h>

#define FRAMEBUFFER_DESIRED_LOCATION ToCanonicalAddress((UINT64)(PDP_COVERED_SIZE * (KERNEL_DESIRED_PML4_ENTRY) + 256 * PD_COVERED_SIZE))

#define STACK_SIZE 16 * PAGE_SIZE

#endif