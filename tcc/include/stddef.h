#ifndef _STDDEF_H
#define _STDDEF_H
#define NULL 0
typedef int ptrdiff_t;
typedef unsigned int size_t;
typedef int wchar_t;
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif
