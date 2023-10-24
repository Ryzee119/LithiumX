// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_LAUNCHER_H
#define _DASH_LAUNCHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

typedef struct xbe_launch_param
{
    char title[MAX_META_LEN];
    char title_id[MAX_META_LEN];
    char selected_path[256];
} launch_param_t;

bool dash_launcher_is_xbe(const char *file_path, char *title, char *title_id);
bool dash_launcher_is_iso(const char *file_path, char *title, char *title_id);

bool dash_launcher_is_launchable(const char *file_path);
void dash_launcher_go(const char *selected_path);
#ifdef __cplusplus
}
#endif

#endif
