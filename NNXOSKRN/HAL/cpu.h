#ifndef NNX_CPU_HEADER
#define NNX_CPU_HEADER

#include <nnxtype.h>

typedef CHAR	KPROCESSOR_MODE;

#define UserMode 1
#define KernelMode 0

#define KAFFINITY_ALL 0xFFFFFFFFFFFFFFFFULL
typedef ULONG_PTR	KAFFINITY;

#endif
