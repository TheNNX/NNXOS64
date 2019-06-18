#pragma once
#ifndef __NNXARG_H
#define	__NNXARG_H

#ifndef __NNX_LIST_H
#define	__NNX_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif
	typedef unsigned char *va_list;

#ifdef __cplusplus
}
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct __bigstruct { int a[2]; }	STACKITEM;
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

