#ifndef NNX_MP_HEADER
#define NNX_MP_HEADER

#ifdef __cplusplus
extern "C"{
#endif

extern UINT KeNumberOfProcessors;
ULONG KeGetCurrentProcessorId();

#ifdef __cplusplus
}
#endif

#endif