/* Timecop-style annotations (public domain pattern; see Timecop docs). */
#ifndef WEAVERKEM_POISON_H
#define WEAVERKEM_POISON_H

#include <valgrind/memcheck.h>

#define poison(addr, len) VALGRIND_MAKE_MEM_UNDEFINED((addr), (len))
#define unpoison(addr, len) VALGRIND_MAKE_MEM_DEFINED((addr), (len))

#endif
