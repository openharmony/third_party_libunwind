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

#ifndef LIBUNWIND_I_OHOS_H
#define LIBUNWIND_I_OHOS_H
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "libunwind_i.h"
#include "map_info.h"

// Unwind using specific method
// target dependent
extern int uwn_step(unw_cursor_t *, int method);
// Get relative pc from cursor or calculate it
extern unw_word_t unw_get_rel_pc (unw_cursor_t *);
// We may want to get previous executed address, thus previous instruction size is needed
// target dependent
extern unw_word_t unw_get_previous_instr_sz(unw_cursor_t *);
// Get current associated map
extern struct map_info* unw_get_map (unw_cursor_t *);
// Get map info list of current unwind target
extern struct map_info* unw_get_maps (unw_cursor_t *);
// Loop the symbol table to find matched symbol
extern int unw_get_symbol_info(struct unw_cursor *cursor, uint64_t pc, int buf_sz, char *buf, uint64_t *sym_start, uint64_t *sym_end);
// Set unwind process id, for caching maps
extern void unw_set_target_pid(unw_addr_space_t as, int pid);

// Create caching maps for local uinwinding
extern void unw_init_local_address_space(unw_addr_space_t* as);
// Destory caching maps for local uinwinding
extern void unw_destroy_local_address_space(unw_addr_space_t as);
// Init cursor with other address space
extern int unw_init_local_with_as(unw_addr_space_t as, unw_cursor_t *cursor, unw_context_t *uc);
// Loop the symbol table to find matched symbol, only for local uinwinding
extern int unw_get_symbol_info_by_pc(unw_addr_space_t as, uint64_t pc, int buf_sz, char *buf, uint64_t *sym_start, uint64_t *sym_end);

#ifdef __cplusplus
}
#endif
#endif
