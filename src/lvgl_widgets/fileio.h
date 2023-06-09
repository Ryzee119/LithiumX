// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _FILEIO_H
#define _FILEIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

bool lv_fs_exists(const char *path);
uint32_t lv_fs_size(lv_fs_file_t *fp);
uint8_t *lv_fs_orc(const char *fn, uint32_t *br);

#ifdef __cplusplus
}
#endif

#endif
