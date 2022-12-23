/* libunwind - a platform-independent unwind library
   Copyright (C) 2003-2005 Hewlett-Packard Co
        Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

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

#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "libunwind_i.h"
#include "map_info.h"
#include "os-linux.h"

struct map_info *
maps_create_list(pid_t pid)
{
  struct map_iterator mi;
  unsigned long start, end, offset, flags;
  struct map_info *map_list = NULL;
  struct map_info *cur_map;
  struct map_info *buf;
  int sz;
  int index = 0;
  if ((sz = maps_init (&mi, pid)) < 0)
    return NULL;

  if (sz < 0 || sz > 65536) {
    return NULL;
  }

  int buf_sz = sz + 256;
  buf = (struct map_info*)mmap(NULL, buf_sz * sizeof(struct map_info), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (buf == NULL) {
    return NULL;
  }

  while (maps_next (&mi, &start, &end, &offset, &flags))
    {
      if (index >= buf_sz) {
        Dprintf("Lost Map:%p-%p %s\n", (void*)start, (void*)end, mi.path);
        continue;
      }

      cur_map = &buf[index];
      cur_map->next = map_list;
      cur_map->start = start;
      cur_map->end = end;
      cur_map->offset = offset;
      cur_map->flags = flags;
      cur_map->path = strdup(mi.path);
      cur_map->ei.size = 0;
      cur_map->ei.image = NULL;
      cur_map->ei.has_dyn_info = 0;
      cur_map->ei.load_bias = -1;
      cur_map->ei.strtab = NULL;
      cur_map->sz = sz;
      cur_map->buf = buf;
      cur_map->buf_sz = buf_sz;
      map_list = cur_map;
      index = index + 1;
    }
  if (map_list != NULL) {
      map_list->sz = index;
  } else {
      munmap(buf, buf_sz * sizeof(struct map_info));
  }
  maps_close (&mi);
  Debug(12, "Finish create map list, sz:%d, index%d.\n", sz, index);
  return map_list;
}

void
maps_destroy_list(struct map_info *map_info)
{
  struct map_info *map;
  int buf_sz  = map_info->buf_sz;
  void* buf = map_info->buf;
  while (map_info)
    {
      map = map_info;
      map_info = map->next;
      if (map->ei.image != MAP_FAILED && map->ei.image != NULL) {
        munmap(map->ei.image, map->ei.size);
        map->ei.image = NULL;
      }

      if (map->mdi.image != MAP_FAILED && map->mdi.image != NULL) {
        munmap(map->mdi.image, map->mdi.size);
        map->mdi.image = NULL;
      }

      if (map->path) {
        free(map->path);
        map->path = NULL;
      }
      map = NULL;
    }
  if (buf != NULL) {
    munmap(buf, buf_sz * sizeof(struct map_info));
    buf = NULL;
  }
}

struct map_info *
get_map(struct map_info *map_list, unw_word_t addr)
{
  if (map_list == NULL) {
    return NULL;
  }

  struct map_info* buf = map_list->buf;
  if (buf == NULL) {
    return NULL;
  }

  int begin = 0;
  int end = map_list->sz - 1;
  while (begin <= end) {
    int mid = begin + ((end - begin) / 2);
    if (addr < buf[mid].start) {
      end = mid - 1;
    } else if (addr <= buf[mid].end) {
      return &buf[mid];
    } else {
      begin = mid + 1;
    }
  }

  if ((addr >= buf[begin].start) && (addr <= buf[begin].end)) {
    return &buf[begin];
  }

  Dprintf("Could not find map for addr:%p\n", (void*)addr);
  return NULL;
}

int maps_is_readable(struct map_info *map_list, unw_word_t addr)
{
  /* If there is no map, assume everything is okay. */
  if (map_list == NULL)
    return 1;
  struct map_info *map = get_map(map_list, addr);
  if (map != NULL)
    return ((map->flags & PROT_READ) && (addr <= (map->end - sizeof(unw_word_t))));
  return 0;
}

int maps_is_writable(struct map_info *map_list, unw_word_t addr)
{
  /* If there is no map, assume everything is okay. */
  if (map_list == NULL)
    return 1;
  struct map_info *map = get_map(map_list, addr);
  if (map != NULL)
    return ((map->flags & PROT_WRITE) && (addr <= (map->end - sizeof(unw_word_t))));
  return 0;
}

struct map_info*
tdep_get_elf_image(unw_addr_space_t as, pid_t pid, unw_word_t ip)
{
  struct map_info *map;
  struct cursor* cursor = get_cursor_from_as(as);
  int find_cached_map = ((cursor != NULL) && (cursor->dwarf.ip == ip));
  if (find_cached_map  &&
    (cursor->dwarf.ip == cursor->dwarf.cached_ip) && cursor->dwarf.cached_map != NULL) {
    return cursor->dwarf.cached_map;
  }

  if (as->map_list == NULL && pid > 0) {
    as->map_list = maps_create_list(pid);
    if (as->map_list == NULL) {
      Dprintf("Failed to maps_create_list for pid:%d\n", pid);
      return NULL;
    }
  }
    

  map = get_map(as->map_list, ip);
  if (!map)
    return NULL;

  if (map->ei.image == NULL)
    {
      int ret = elf_map_image(&map->ei, map->path);
      if (ret < 0)
        {
          map->ei.image = NULL;
          map->ei.has_try_load = 1;
          Dprintf("Failed to elf_map_image for ip:%p\n", (void*)ip);
          return NULL;
        }
      map->ei.mdi = &(map->mdi);
    }

  if (find_cached_map) {
    cursor->dwarf.cached_map = map;
    cursor->dwarf.cached_ip = ip;
  }
  return map;
}

unw_word_t get_previous_instr_sz(unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  unw_addr_space_t as = c->dwarf.as;
  unw_accessors_t *a = unw_get_accessors (as);
  unw_word_t ip = c->dwarf.ip;
  int sz = 4;
#if defined(UNW_TARGET_ARM)
  if (ip)
    {
      if (ip & 1)
        {
          void *arg;
          unw_word_t value;
          arg = c->dwarf.as_arg;
          // 0xe000f000 ---> machine code of blx Instr (blx label)
          if (ip < 5 || (*a->access_mem) (as, ip - 5, &value, 0, arg) < 0 ||
              (value & 0xe000f000) != 0xe000f000)
            sz = 2;
        }
    }
#elif defined(UNW_TARGET_ARM64)
  sz = 4;
#elif defined(UNW_TARGET_X86)
  sz = 1;
#elif defined(UNW_TARGET_x86_64)
  sz = 1;
#else
// other arch need to be add here.
#endif
  return sz;
}


#ifndef UNW_REMOTE_ONLY

void
tdep_get_exe_image_path (char *path)
{
  strcpy(path, "/proc/self/exe");
}

#endif /* !UNW_REMOTE_ONLY */
