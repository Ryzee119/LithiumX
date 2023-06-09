// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _WIDGET_MENU_H
#define _WIDGET_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lithiumx.h"

typedef void (*menuitem_cb_t)(void *param);

typedef struct menu_items
{
    const char *str;  // Name of the menu item
    menuitem_cb_t cb; // The function callback that is called on pressed
    void *callback_param;
    const char* confirm_box;
} menu_items_t;

lv_obj_t *menu_open(menu_items_t *menu_items, int cnt);
lv_obj_t *menu_open_static(const menu_items_t *menu_items, int cnt);
void menu_force_value(lv_obj_t *menu, int row);

#ifdef __cplusplus
}
#endif

#endif