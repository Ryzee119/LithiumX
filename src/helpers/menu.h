// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _MENU_H
#define _MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_WIDTH (LV_MIN(600,lv_obj_get_width(lv_scr_act()) * 2 / 3))
#define MENU_HEIGHT (LV_MIN(440,lv_obj_get_height(lv_scr_act()) * 2 / 3))

typedef void (*confirm_cb_t)(void);

typedef struct
{
    lv_obj_t *old_focus;
    lv_obj_t *old_focus_parent;
    confirm_cb_t cb;
} menu_data_t;

void menu_apply_style(lv_obj_t *obj);
void menu_table_scroll(lv_event_t *e);
void menu_show_item(lv_obj_t *obj, confirm_cb_t confirmbox_cb);
void menu_hide_item(lv_obj_t *obj);
void confirmbox_open(confirm_cb_t confirmbox_cb, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
