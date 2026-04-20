/* Timecop-style annotations (public domain pattern; see Timecop docs). */
#ifndef WEAVERKEM_POISON_H
#define WEAVERKEM_POISON_H

#ifdef __has_include
#if __has_include(<valgrind/memcheck.h>)
#include <valgrind/memcheck.h>
#define HAS_VALGRIND
#endif
#endif

#ifdef HAS_VALGRIND
#define poison(addr, len) VALGRIND_MAKE_MEM_UNDEFINED((addr), (len))
#define unpoison(addr, len) VALGRIND_MAKE_MEM_DEFINED((addr), (len))
#else
#define poison(addr, len) ((void)0)
#define unpoison(addr, len) ((void)0)
#endif

#endif
