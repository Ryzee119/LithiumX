// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _SYNOP_H
#define _SYNOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dash_titlelist.h"

void synop_menu_init(void);
void synop_menu_deinit(void);
void synop_menu_open(title_t *title);

#ifdef __cplusplus
}
#endif

#endif
