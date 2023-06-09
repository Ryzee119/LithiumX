// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "dash.h"
#include "dash_styles.h"
#include "helpers/menu.h"
#include "helpers/nano_debug.h"

enum
{
    SUBMENU_CANCEL,
    SUBMENU_ACCEPT,
};

void menu_apply_style(lv_obj_t *obj)
{
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(obj, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_add_style(obj, &menu_table_highlight_style, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// Scroll the main menu table if the selected cell is outside of the container view
void menu_table_scroll(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);

    if (lv_obj_check_type(obj, &lv_table_class) == false)
    {
        return;
    }

    uint16_t row, col;
    int row_height, top, sel, bot, scroll;

    lv_table_get_selected_cell(obj, &row, &col);

    row_height = lv_font_get_line_height(lv_obj_get_style_text_font(obj, LV_PART_ITEMS)) +
                     lv_obj_get_style_pad_top(obj, LV_PART_ITEMS) +
                     lv_obj_get_style_pad_bottom(obj, LV_PART_ITEMS);

    top = lv_obj_get_y(obj);
    sel = top - lv_obj_get_scroll_y(obj) + (row * row_height);
    bot = lv_obj_get_y2(obj) - row_height;

    scroll = (sel < top) ? (top - sel) :
             (bot < sel) ? (bot - sel) : 0;

    lv_obj_scroll_by_bounded(obj, 0, scroll, LV_ANIM_ON);
}

void menu_show_item(lv_obj_t *obj, confirm_cb_t confirmbox_cb)
{
    lv_group_t *gp = lv_group_get_default();
    menu_data_t *user_data = (menu_data_t *)obj->user_data;
    user_data->cb = confirmbox_cb;
    user_data->old_focus = lv_group_get_focused(gp);
    user_data->old_focus_parent = lv_obj_get_parent(user_data->old_focus);
    lv_group_focus_freeze(gp, false);
    lv_group_focus_obj(obj);
    lv_group_focus_freeze(gp, true);
    lv_obj_move_foreground(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void menu_hide_item(lv_obj_t *obj)
{
    lv_group_t *gp = lv_group_get_default();
    menu_data_t *user_data = (menu_data_t *)obj->user_data;
    lv_group_focus_freeze(gp, false);
    if (lv_obj_is_valid(user_data->old_focus))
    {
        lv_group_focus_obj(user_data->old_focus);
    }
    else
    {
        lv_group_focus_obj(user_data->old_focus_parent);
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// Callback when submenu/confirmbox has an item selected (Either cancel or accept action)
static void confirmbox_callback(lv_event_t *e)
{
    uint16_t row, col;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    menu_data_t *user_data = (menu_data_t *)obj->user_data;
    lv_table_get_selected_cell(obj, &row, &col);
    if (key == LV_KEY_ENTER)
    {
        if (row == SUBMENU_ACCEPT && user_data->cb)
        {
            user_data->cb();
        }
        menu_hide_item(obj);
    }
    else if (key == LV_KEY_ESC || key == DASH_SETTINGS_PAGE)
    {
        menu_hide_item(obj);
    }
}

void confirmbox_open(confirm_cb_t confirmbox_cb, const char *format, ...)
{
    static lv_obj_t *confirm_box = NULL;
    lv_group_t *gp = lv_group_get_default();

    //On first call, create the confirmbox
    if (confirm_box == NULL)
    {
        // Create a table for the confirmation box
        confirm_box = lv_table_create(lv_scr_act());
        menu_data_t *user_data = (menu_data_t *)lv_mem_alloc(sizeof(menu_data_t));
        lv_memset(user_data, 0, sizeof(menu_data_t));
        confirm_box->user_data = user_data;
        menu_apply_style(confirm_box);
        lv_table_set_cell_value(confirm_box, 0, 0, "Cancel");
        lv_table_set_col_width(confirm_box, 0, MENU_WIDTH);
        lv_obj_add_event_cb(confirm_box, confirmbox_callback, LV_EVENT_KEY, NULL);
        lv_group_add_obj(gp, confirm_box);
        lv_obj_add_flag(confirm_box, LV_OBJ_FLAG_HIDDEN);
    }

    char buf[256];
    va_list args;
    va_start(args, format);
    lv_vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    lv_table_set_cell_value_fmt(confirm_box, 1, 0, buf);
    menu_show_item(confirm_box, confirmbox_cb);
    lv_table_t *t = (lv_table_t *)confirm_box;
    t->row_act = 0;
}
