#ifndef NNX_ARG_HEADER
#define	NNX_ARG_HEADER


#ifdef __cplusplus

extern "C"
{
#endif

#include <intrin.h>

typedef INT64 STACKITEM;
#define	VA_SIZE(TYPE)					\
	((sizeof(TYPE) + sizeof(STACKITEM) - 1)	\
		& ~(sizeof(STACKITEM) - 1))

#define	va_start(AP, LASTARG)	\
	(AP=((va_list)&(LASTARG) + VA_SIZE(LASTARG)))

#define va_end(AP)

#define va_arg(AP, TYPE)	\
	(AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))

#ifdef __cplusplus
}
#endif
#endif