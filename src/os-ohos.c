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

#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "elfxx.h"
#include "libunwind_i.h"

#define unw_init_local_addr_space UNW_OBJ(init_local_addr_space)

extern unw_word_t get_previous_instr_sz(unw_cursor_t *cursor);
extern void unw_init_local_addr_space (unw_addr_space_t as);
extern struct map_info *get_map(struct map_info *map_list, unw_word_t addr);

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
  return get_map(c->dwarf.as->map_list, c->dwarf.ip);
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
  int pid = -1;
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
unw_set_context(unw_cursor_t * cursor, uintptr_t regs[], int reg_sz)
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
#elif defined(__x86_64__)
  c->dwarf.ip = regs[UNW_REG_IP];
  c->dwarf.cfa = regs[UNW_REG_SP];
#endif
  return;
}

void
unw_set_adjust_pc(struct unw_cursor *cursor, uint64_t pc)
{
  struct cursor *c = (struct cursor *) cursor;
  c->dwarf.ip = pc;
}

bool
unw_is_ark_managed_frame(struct cursor* c)
{
  if (c->dwarf.cached_map != NULL) {
    Dprintf("cached map with elf image, not ark frame.\n");
    return false;
  }

  struct map_info* map = get_map(c->dwarf.as->map_list, c->dwarf.ip);
  if (map == NULL) {
    Dprintf("Not mapped ip.\n");
    return false;
  }

  if ((strstr(map->path, "[anon:ArkJS Heap]") == NULL) &&
      (strstr(map->path, "/dev/zero") == NULL)) {
    Dprintf("Not ark map:%s.\n", map->path);
    return false;
  }

  if ((map->flags & PROT_EXEC) == 0) {
    Dprintf("Target map is not executable.\n");
    return false;
  }

  return true;
}

#define ARK_LIB_NAME "libark_jsruntime.so"
int (*step_ark_managed_native_frame_fn)(int, uintptr_t*, uintptr_t*, uintptr_t*, char*, size_t);
int (*get_ark_js_heap_crash_info_fn)(int, uintptr_t *, uintptr_t *, int, char *, size_t);
void* handle = NULL; // this handle will never be unloaded
pthread_mutex_t lock;

int
unw_step_ark_managed_native_frame(int pid, uintptr_t* pc, uintptr_t* fp, uintptr_t* sp, char* buf, size_t buf_sz)
{
  if (step_ark_managed_native_frame_fn != NULL) {
    return step_ark_managed_native_frame_fn(pid, pc, fp, sp, buf, buf_sz);
  }

  pthread_mutex_lock(&lock);
  if (step_ark_managed_native_frame_fn != NULL) {
    pthread_mutex_unlock(&lock);
    return step_ark_managed_native_frame_fn(pid, pc, fp, sp, buf, buf_sz);
  }

  if (handle == NULL) {
    handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
    if (handle == NULL) {
      Dprintf("Failed to load library(%s).\n", dlerror());
      pthread_mutex_unlock(&lock);
      return -1;
    }
  }

  *(void**)(&step_ark_managed_native_frame_fn) = dlsym(handle, "step_ark_managed_native_frame");
  if (!step_ark_managed_native_frame_fn) {
    Dprintf("Failed to find symbol(%s).\n", dlerror());
    handle = NULL;
    pthread_mutex_unlock(&lock);
    return -1;
  }

  pthread_mutex_unlock(&lock);
  return step_ark_managed_native_frame_fn(pid, pc, fp, sp, buf, buf_sz);
}

int
unw_get_ark_js_heap_crash_info(int pid, uintptr_t* x20, uintptr_t* fp, int out_js_info, char* buf, size_t buf_sz)
{
  if (get_ark_js_heap_crash_info_fn != NULL) {
    return get_ark_js_heap_crash_info_fn(pid, x20, fp, out_js_info, buf, buf_sz);
  }

  pthread_mutex_lock(&lock);
  if (get_ark_js_heap_crash_info_fn != NULL) {
    pthread_mutex_unlock(&lock);
    return get_ark_js_heap_crash_info_fn(pid, x20, fp, out_js_info, buf, buf_sz);
  }

  if (handle == NULL) {
    handle = dlopen(ARK_LIB_NAME, RTLD_LAZY);
    if (handle == NULL) {
      Dprintf("Failed to load library(%s).\n", dlerror());
      pthread_mutex_unlock(&lock);
      return -1;
    }
  }

  *(void**)(&get_ark_js_heap_crash_info_fn) = dlsym(handle, "get_ark_js_heap_crash_info");
  if (!get_ark_js_heap_crash_info_fn) {
    Dprintf("Failed to find symbol(%s).\n", dlerror());
    handle = NULL;
    pthread_mutex_unlock(&lock);
    return -1;
  }

  pthread_mutex_unlock(&lock);
  return get_ark_js_heap_crash_info_fn(pid, x20, fp, out_js_info, buf, buf_sz);
}

bool
unw_get_build_id (struct map_info* map, uint8_t** build_id_ptr, size_t* length)
{
#ifdef PARSE_BUILD_ID
  if (map == NULL) {
    return false;
  }

  if (map->ei.build_id_note == NULL) {
    return false;
  }

  *build_id_ptr =  map->ei.build_id_note->build_id;
  *length = map->ei.build_id_note->nhdr.n_descsz;
  return true;
#else
  return false;
#endif
}
