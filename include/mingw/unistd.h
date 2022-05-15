// This is an incomplete & imprecice implementation of the Posix
// standard file by the same name

#ifndef __MINGW_UNISTD__
#define __MINGW_UNISTD__

#ifdef MINGW // Only for cross compilation to mingw

#include <stdio.h>

#ifndef UNW_REMOTE_ONLY
// This is solely intended to enable compilation of libunwind
// for UNW_REMOTE_ONLY on windows
#error Cross compilation of libunwind on Windows can only support UNW_REMOTE_ONLY
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>


int          getpagesize(void);
// mingw have these
// int          close(int);
// int          open(const char *, int, ...);
// ssize_t      read(int fd, void *buf, size_t count);
// ssize_t      write(int, const void *, size_t);

// read write is here
#include <io.h>

#endif // MINGW
#endif