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

#ifndef OS_OHOS_H
#define OS_OHOS_H

#include <inttypes.h>
#include "libunwind_i.h"

struct map_info;
struct unw_cursor;

typedef void (*symbol_callback)(uint64_t start_offset, uint64_t end_offset, uint64_t str_offset);
extern unw_word_t unw_get_rel_pc (struct unw_cursor *cursor);
extern unw_word_t unw_get_previous_instr_sz(unw_cursor_t *cursor);
extern struct map_info* unw_get_map (struct unw_cursor *cursor);
extern struct map_info* unw_get_maps (struct unw_cursor *cursor);
extern int unw_iterator_elf_symbols(struct unw_cursor *cursor, unw_word_t ip, symbol_callback cb);
extern int unw_get_proc_name_by_offset(struct unw_cursor *cursor,  uint64_t str_offset, char* buf, int sz);
extern void unw_set_target_pid(unw_addr_space_t as, int pid);

#endif
