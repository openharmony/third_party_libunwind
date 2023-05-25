/* libunwind - a platform-independent unwind library
   Copyright (C) 2008 CodeSourcery

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <stdatomic.h>

#include "unwind_i.h"

#ifdef UNW_REMOTE_ONLY

/* unw_local_addr_space is a NULL pointer in this case.  */
unw_addr_space_t unw_local_addr_space;

#else /* !UNW_REMOTE_ONLY */

static struct unw_addr_space local_addr_space;

unw_addr_space_t unw_local_addr_space = &local_addr_space;

static inline void *
uc_addr (unw_tdep_context_t *uc, int reg)
{
  if (reg >= UNW_ARM_R0 && reg < UNW_ARM_R0 + 16)
    return &uc->regs[reg - UNW_ARM_R0];
  else
    return NULL;
}

# ifdef UNW_LOCAL_ONLY

HIDDEN void *
tdep_uc_addr (unw_tdep_context_t *uc, int reg)
{
  return uc_addr (uc, reg);
}

# endif /* UNW_LOCAL_ONLY */

static int
get_dyn_info_list_addr (unw_addr_space_t as, unw_word_t *dyn_info_list_addr,
                        void *arg)
{
#ifndef UNW_LOCAL_ONLY
# pragma weak _U_dyn_info_list_addr
  if (!_U_dyn_info_list_addr)
    return -UNW_ENOINFO;
#endif
  // Access the `_U_dyn_info_list` from `LOCAL_ONLY` library, i.e. libunwind.so.
  *dyn_info_list_addr = _U_dyn_info_list_addr ();
  return 0;
}

#define PAGE_SIZE 4096
#define PAGE_START(a)	((a) & ~(PAGE_SIZE-1))

static int mem_validate_pipe[2] = {-1, -1};

#ifdef HAVE_PIPE2
static inline void
do_pipe2 (int pipefd[2])
{
  pipe2 (pipefd, O_CLOEXEC | O_NONBLOCK);
}
#else
static inline void
set_pipe_flags (int fd)
{
  int fd_flags = fcntl (fd, F_GETFD, 0);
  int status_flags = fcntl (fd, F_GETFL, 0);

  fd_flags |= FD_CLOEXEC;
  fcntl (fd, F_SETFD, fd_flags);

  status_flags |= O_NONBLOCK;
  fcntl (fd, F_SETFL, status_flags);
}

static inline void
do_pipe2 (int pipefd[2])
{
  pipe (pipefd);
  set_pipe_flags(pipefd[0]);
  set_pipe_flags(pipefd[1]);
}
#endif

static inline void
open_pipe (void)
{
  if (mem_validate_pipe[0] != -1)
    close (mem_validate_pipe[0]);
  if (mem_validate_pipe[1] != -1)
    close (mem_validate_pipe[1]);

  do_pipe2 (mem_validate_pipe);
}

ALWAYS_INLINE
static int
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
__attribute__((no_sanitize("address")))
#endif
#endif
write_validate (void *addr)
{
  int ret = -1;
  ssize_t bytes = 0;

  do
    {
      char buf;
      bytes = syscall(SYS_read, mem_validate_pipe[0], &buf, 1);
    }
  while ( errno == EINTR );

  int valid_read = (bytes > 0 || errno == EAGAIN || errno == EWOULDBLOCK);
  if (!valid_read)
    {
      // re-open closed pipe
      open_pipe ();
    }

  do
    {
       ret = syscall(SYS_write, mem_validate_pipe[1], addr, 1);
    }
  while ( errno == EINTR );

  return ret;
}

static int (*mem_validate_func) (void *addr, size_t len);
static int msync_validate (void *addr, size_t len)
{
  if (msync (addr, len, MS_ASYNC) != 0)
    {
      return -1;
    }

  return write_validate (addr);
}

#ifdef HAVE_MINCORE
static int mincore_validate (void *addr, size_t len)
{
  unsigned char mvec[2]; /* Unaligned access may cross page boundary */

  /* mincore could fail with EAGAIN but we conservatively return -1
     instead of looping. */
  if (mincore (addr, len, (unsigned char *)mvec) != 0)
    {
    return -1;
    }

  return write_validate (addr);
}
#endif

/* Initialise memory validation method. On linux kernels <2.6.21,
   mincore() returns incorrect value for MAP_PRIVATE mappings,
   such as stacks. If mincore() was available at compile time,
   check if we can actually use it. If not, use msync() instead. */
HIDDEN void
tdep_init_mem_validate (void)
{
  open_pipe ();

#ifdef HAVE_MINCORE
  unsigned char present = 1;
  unw_word_t addr = PAGE_START((unw_word_t)&present);
  unsigned char mvec[1];
  int ret;
  while ((ret = mincore ((void*)addr, PAGE_SIZE, (unsigned char *)mvec)) == -1 &&
         errno == EAGAIN) {}
  if (ret == 0)
    {
      Debug(1, "using mincore to validate memory\n");
      mem_validate_func = mincore_validate;
    }
  else
#endif
    {
      Debug(1, "using msync to validate memory\n");
      mem_validate_func = msync_validate;
    }
}

/* Cache of already validated addresses */
#define NLGA 4
#if defined(HAVE___CACHE_PER_THREAD) && HAVE___CACHE_PER_THREAD
// thread-local variant
static _Thread_local unw_word_t last_good_addr[NLGA];
static _Thread_local int lga_victim;

static int
is_cached_valid_mem(unw_word_t addr)
{
  int i;
  for (i = 0; i < NLGA; i++)
    {
      if (addr == last_good_addr[i])
        return 1;
    }
      return 0;
    }

static void
cache_valid_mem(unw_word_t addr)
{
  int i, victim;
  victim = lga_victim;
  for (i = 0; i < NLGA; i++) {
    if (last_good_addr[victim] == 0) {
      last_good_addr[victim] = addr;
      return;
    }
    victim = (victim + 1) % NLGA;
  }

  /* All slots full. Evict the victim. */
  last_good_addr[victim] = addr;
  victim = (victim + 1) % NLGA;
  lga_victim = victim;
}

#else
// global, thread safe variant
static _Atomic unw_word_t last_good_addr[NLGA];
static _Atomic int lga_victim;

static int
is_cached_valid_mem(unw_word_t addr)
{
  int i;
  for (i = 0; i < NLGA; i++)
    {
      if (addr == atomic_load(&last_good_addr[i]))
        return 1;
    }
  return 0;
}

static void
cache_valid_mem(unw_word_t addr)
{
  int i, victim;
  victim = atomic_load(&lga_victim);
  unw_word_t zero = 0;
  for (i = 0; i < NLGA; i++) {
    if (atomic_compare_exchange_strong(&last_good_addr[victim], &zero, addr)) {
      return;
    }
    victim = (victim + 1) % NLGA;
  }

  /* All slots full. Evict the victim. */
  atomic_store(&last_good_addr[victim], addr);
    victim = (victim + 1) % NLGA;
  atomic_store(&lga_victim, victim);
}
#endif

static int
validate_mem (unw_word_t addr)
{
  size_t len;

  if (PAGE_START(addr + sizeof (unw_word_t) - 1) == PAGE_START(addr))
    len = PAGE_SIZE;
  else
    len = PAGE_SIZE * 2;

  addr = PAGE_START(addr);

  if (addr == 0)
      return -1;

  if (is_cached_valid_mem(addr))
    return 0;

  if (mem_validate_func ((void *) addr, len) == -1)
    return -1;

  cache_valid_mem(addr);

  return 0;
}

static int
access_mem (unw_addr_space_t as, unw_word_t addr, unw_word_t *val, int write,
            void *arg)
{
  /* validate address */
    const struct cursor *c = (const struct cursor *) arg;
    if (c && validate_mem(addr))
      return -1;

  if (write)
    {
#ifdef IS_VALIDATE_MEM
      if (maps_is_writable(as->map_list, addr)) {
#endif
        Debug (16, "mem[%x] <- %x\n", addr, *val);
        *(unw_word_t *) addr = *val;
#ifdef IS_VALIDATE_MEM
      } else {
        Debug (16, "Unwritable memory mem[%lx] <- %lx\n", addr, *val);
        return -1;
      }
#endif
    }
  else
    {
#ifdef IS_VALIDATE_MEM
      if (maps_is_readable(as->map_list, addr)) {
#endif
        *val = *(unw_word_t *) addr;
        Debug (16, "mem[%x] -> %x\n", addr, *val);
#ifdef IS_VALIDATE_MEM
      } else {
        Debug (16, "Unreadable memory mem[%lx] <- %lx\n", addr, *val);
        return -1;
      }
#endif
    }
  return 0;
}

static int
access_reg (unw_addr_space_t as, unw_regnum_t reg, unw_word_t *val, int write,
            void *arg)
{
  unw_word_t *addr;
  unw_tdep_context_t *uc = arg;

  if (unw_is_fpreg (reg))
    goto badreg;

Debug (16, "reg = %s\n", unw_regname (reg));
  if (!(addr = uc_addr (uc, reg)))
    goto badreg;

  if (write)
    {
      *(unw_word_t *) addr = *val;
      Debug (12, "%s <- %x\n", unw_regname (reg), *val);
    }
  else
    {
      *val = *(unw_word_t *) addr;
      Debug (12, "%s -> %x\n", unw_regname (reg), *val);
    }
  return 0;

 badreg:
  Debug (1, "bad register number %u\n", reg);
  return -UNW_EBADREG;
}

static int
access_fpreg (unw_addr_space_t as, unw_regnum_t reg, unw_fpreg_t *val,
              int write, void *arg)
{
  unw_tdep_context_t *uc = arg;
  unw_fpreg_t *addr;

  if (!unw_is_fpreg (reg))
    goto badreg;

  if (!(addr = uc_addr (uc, reg)))
    goto badreg;

  if (write)
    {
      Debug (12, "%s <- %08lx.%08lx.%08lx\n", unw_regname (reg),
             ((long *)val)[0], ((long *)val)[1], ((long *)val)[2]);
      *(unw_fpreg_t *) addr = *val;
    }
  else
    {
      *val = *(unw_fpreg_t *) addr;
      Debug (12, "%s -> %08lx.%08lx.%08lx\n", unw_regname (reg),
             ((long *)val)[0], ((long *)val)[1], ((long *)val)[2]);
    }
  return 0;

 badreg:
  Debug (1, "bad register number %u\n", reg);
  /* attempt to access a non-preserved register */
  return -UNW_EBADREG;
}

static int
get_static_proc_name (unw_addr_space_t as, unw_word_t ip,
                      char *buf, size_t buf_len, unw_word_t *offp,
                      void *arg)
{
  return _Uelf32_get_proc_name (as, getpid (), ip, buf, buf_len, offp);
}

HIDDEN void
arm_local_addr_space_init (void)
{
  memset (&local_addr_space, 0, sizeof (local_addr_space));
  local_addr_space.caching_policy = UNW_CACHE_GLOBAL;
  local_addr_space.acc.find_proc_info = arm_find_proc_info;
  local_addr_space.acc.put_unwind_info = arm_put_unwind_info;
  local_addr_space.acc.get_dyn_info_list_addr = get_dyn_info_list_addr;
  local_addr_space.acc.access_mem = access_mem;
  local_addr_space.acc.access_reg = access_reg;
  local_addr_space.acc.access_fpreg = access_fpreg;
  local_addr_space.acc.resume = arm_local_resume;
  local_addr_space.acc.get_proc_name = get_static_proc_name;
  unw_flush_cache (&local_addr_space, 0, 0);
}

HIDDEN void
init_local_addr_space (unw_addr_space_t as)
{
  memset (as, 0, sizeof (struct unw_addr_space));
  as->caching_policy = UNW_CACHE_GLOBAL;
  as->acc.find_proc_info = arm_find_proc_info;
  as->acc.put_unwind_info = arm_put_unwind_info;
  as->acc.get_dyn_info_list_addr = get_dyn_info_list_addr;
  as->acc.access_mem = access_mem;
  as->acc.access_reg = access_reg;
  as->acc.access_fpreg = access_fpreg;
  as->acc.resume = arm_local_resume;
  as->acc.get_proc_name = get_static_proc_name;
  unw_flush_cache (as, 0, 0);
}

#endif /* !UNW_REMOTE_ONLY */
