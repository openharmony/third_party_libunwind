/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os-ohos.h"

#include <unistd.h>

#include "elfxx.h"
#include "libunwind_i.h"

#define unw_init_local_addr_space UNW_OBJ(init_local_addr_space)

extern unw_word_t get_previous_instr_sz(unw_cursor_t *cursor);
extern void unw_init_local_addr_space (unw_addr_space_t as);

void 
unw_set_target_pid(unw_addr_space_t as, int pid)
{
  as->pid = pid;
}

unw_word_t
unw_get_rel_pc (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  if (c->dwarf.rel_pc != 0 && c->dwarf.cached_ip == c->dwarf.ip) {
    return c->dwarf.rel_pc;
  }

  c->dwarf.cached_map = tdep_get_elf_image (c->dwarf.as, c->dwarf.as->pid, c->dwarf.ip);
  if (c->dwarf.cached_map == NULL) {
    c->dwarf.cached_ip = 0;
    return 0;
  }

  if (c->dwarf.cached_map->ei.load_bias == -1) {
    struct elf_dyn_info edi;
    invalidate_edi(&edi);
    if (tdep_find_unwind_table (&edi, &(c->dwarf.cached_map->ei),
      c->dwarf.as, c->dwarf.cached_map->path,
      c->dwarf.cached_map->start, c->dwarf.cached_map->offset,
      c->dwarf.ip) < 0) {
      return 0;
    }
  }

  c->dwarf.rel_pc = c->dwarf.ip -
                    c->dwarf.cached_map->start +
                    c->dwarf.cached_map->offset +
                    c->dwarf.cached_map->ei.load_bias;
  return c->dwarf.rel_pc;
}

unw_word_t unw_get_previous_instr_sz(unw_cursor_t *cursor)
{
  return get_previous_instr_sz(cursor);
}

struct map_info*
unw_get_map (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  return tdep_get_elf_image(c->dwarf.as, c->dwarf.as->pid, c->dwarf.ip);
}

struct map_info*
unw_get_maps (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  return c->dwarf.as->map_list;
}

/*
0 success
-1 failed, not found
*/
int
unw_get_symbol_info(struct unw_cursor *cursor, uint64_t pc, int buf_sz, char *buf, uint64_t *sym_start, uint64_t *sym_end)
{
  struct map_info* map = unw_get_map(cursor);
  if (map == NULL) {
    return -1;
  }

  return elf_w (get_symbol_info_in_image)(&(map->ei), map->start, map->offset, pc, buf_sz, buf, sym_start, sym_end);
}

int
unw_get_symbol_info_by_pc(unw_addr_space_t as, uint64_t pc, int buf_sz, char *buf, uint64_t *sym_start, uint64_t *sym_end)
{
#ifdef UNW_REMOTE_ONLY
  return -1;
#else
  if (as->map_list == NULL) {
    return -1;
  }

  struct map_info* map = tdep_get_elf_image(as, as->pid, pc);
  if (map == NULL) {
    return -1;
  }

  return elf_w (get_symbol_info_in_image)(&(map->ei), map->start, map->offset, pc, buf_sz, buf, sym_start, sym_end);
#endif
}

void
unw_init_local_address_space(unw_addr_space_t* as)
{
#ifdef UNW_REMOTE_ONLY
  return;
#else
  if (as == NULL) {
    return;
  }

  if (*as != NULL) {
    return;
  }

  (*as)= (unw_addr_space_t)calloc(1, sizeof(struct unw_addr_space));
  unw_init_local_addr_space(*as);
  int pid = getpid();
  (*as)->pid = pid;
  (*as)->map_list = maps_create_list(pid);
#endif
}

void
unw_destroy_local_address_space(unw_addr_space_t as)
{
#ifdef UNW_REMOTE_ONLY
  return;
#else
  if (as == NULL) {
    return;
  }

  if (as->map_list != NULL) {
    maps_destroy_list(as->map_list);
    as->map_list = NULL;
  }

  free(as);
#endif
}

void
uwn_set_context(unw_cursor_t * cursor, uintptr_t regs[], int reg_sz)
{
  struct cursor *c = (struct cursor *) cursor;
  int min_sz = reg_sz < DWARF_NUM_PRESERVED_REGS ? reg_sz : DWARF_NUM_PRESERVED_REGS;
  c->dwarf.reg_sz = min_sz;
  for (int i = 0; i < min_sz; i++) {
    c->dwarf.ctx[i] = regs[i];
  }

#if defined(__arm__)
  c->dwarf.ip = regs[UNW_ARM_R15];
  c->dwarf.cfa = regs[UNW_REG_SP];
#elif defined(__aarch64__)
  c->dwarf.ip = regs[UNW_AARCH64_PC];
  c->dwarf.cfa = regs[UNW_REG_SP];
#endif
  return;
}