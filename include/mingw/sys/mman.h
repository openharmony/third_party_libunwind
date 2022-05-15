// This is an incomplete & imprecice implementation of the Posix
// standard file by the same name
#ifndef __MINGW_SYS_MMAN__
#define __MINGW_SYS_MMAN__

#ifdef MINGW // Only for cross compilation to mingw

#include <sys/types.h>

#if !HAVE_MMAP

#define MAP_FAILED (void *) -1
#define MAP_ANONYMOUS        1
#define MAP_ANON             MAP_ANONYMOUS
#define MAP_PRIVATE          2

#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4

void* mmap(void *, size_t, int, int, int, size_t);
int   munmap(void *, size_t);
#endif

#endif // MINGW
#endif