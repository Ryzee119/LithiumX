// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _BROWSER_H
#define _BROWSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

typedef bool (*browser_item_selection_cb)(const char *selected_path);

void dash_browser_open(char *path, browser_item_selection_cb cb);

#ifdef __cplusplus
}
#endif

#endif