// This is an incomplete & imprecice implementation
// It defers to the open source freebsd-elf implementations.
#ifndef __MINGW_ELF__
#define __MINGW_ELF__

#ifdef MINGW // Only for cross compilation to mingw

#include <inttypes.h>

#include "freebsd-elf_common.h"
#include "freebsd-elf32.h"
#include "freebsd-elf64.h"

#endif // MINGW
#endif