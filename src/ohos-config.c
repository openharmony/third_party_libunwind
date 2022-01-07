/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This file contains ohos config */

#include "libunwind_i.h"

#define CONF_LINE_SIZE 1024
#define NUMBER_SIXTEEN 16

int config_unwi_debug_level(void)
{
  int ret = 0;
  do {
    FILE *fp = NULL;
    fp = fopen("/system/etc/faultlogger.conf", "r");
    if (fp == NULL) {
      break;
    }

    while (!feof(fp)) {
      char line[CONF_LINE_SIZE] = {0};
      if (fgets(line, CONF_LINE_SIZE - 1, fp) == NULL) {
        continue;
      }

      if (!strncmp("faultlogLogPersist=true", line, strlen("faultlogLogPersist=true"))) {
        ret = NUMBER_SIXTEEN;
      } else if (!strncmp("faultlogLogPersist=false", line, strlen("faultlogLogPersist=false"))) {
        ret = 0;
      }
    }
    fclose(fp);
  } while (0);
  return ret;
}
