// This is an incomplete & imprecice implementation of the Posix
// standard file by the same name
#ifndef __MINGW_SIGNAL__
#define __MINGW_SIGNAL__

#ifdef MINGW // Only for cross compilation to mingw

// Posix is a superset of the ISO C signal.h
// include ISO C version first
#include_next <signal.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#if defined(UNW_TARGET_X86_64)
#  define SIZEOF_SIGINFO 128
#elif defined(UNW_TARGET_ARM64)
#  define SIZEOF_SIGINFO 128
#elif defined(UNW_TARGET_ARM)
#  define SIZEOF_SIGINFO 128
#elif !defined(SIZEOF_SIGINFO)
  // It is not clear whether the sizeof(siginfo_t) is important
  // While compiling on Windows the members are not referenced...
  // However the size maybe important during a case or a memcpy
  // Barring a full audit it could be important so require the size to be defined
#  error SIZEOF_SIGINFO is unknown for this target
#endif

typedef struct siginfo
{
    uint8_t content[SIZEOF_SIGINFO];
} siginfo_t;

typedef long sigset_t;

int          sigfillset(sigset_t *set);

#endif // MINGW
#endif