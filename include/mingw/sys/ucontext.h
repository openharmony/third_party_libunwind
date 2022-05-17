// This is an incomplete & imprecice implementation of the *nix file
// by the same name
#ifndef __MINGW_SYS_UCONTEXT__
#define __MINGW_SYS_UCONTEXT__

#ifdef MINGW // Only for cross compilation to mingw
#include <inttypes.h>

#if defined(UNW_TARGET_X86_64)
#  define SIZEOF_UCONTEXT 936
#elif defined(UNW_TARGET_ARM64)
#  define SIZEOF_UCONTEXT 4560
#elif defined(UNW_TARGET_ARM)
#  define SIZEOF_UCONTEXT 744
#elif !defined(SIZEOF_UCONTEXT)
  // It is not clear whether the sizeof(ucontext_t) is important
  // While compiling on Windows the members are not referenced...
  // However the size maybe important during a case or a memcpy
  // Barring a full audit it could be important so require the size to be defined
#  error SIZEOF_UCONTEXT is unknown for this target
#endif

typedef struct ucontext
{
    uint8_t content[SIZEOF_UCONTEXT];
} ucontext_t;

#ifdef __aarch64__
// These types are used in the definition of the aarch64 unw_tdep_context_t
// They are not used in UNW_REMOTE_ONLY, so typedef them as something
typedef long sigset_t;
typedef long stack_t;

// Windows SDK defines reserved. It conflicts with arm64 ucontext
// Undefine it
#undef __reserved
#endif

#endif // MINGW
#endif