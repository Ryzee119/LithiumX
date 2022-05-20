// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _MENU_H
#define _MENU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*confirm_cb_t)(void);

void main_menu_init(void);
void main_menu_deinit(void);
void main_menu_open(void);
void confirmbox_open(const char *msg, confirm_cb_t confirmbox_cb);

#ifdef __cplusplus
}
#endif

#endif
