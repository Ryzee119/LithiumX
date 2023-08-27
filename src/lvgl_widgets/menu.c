// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "../lithiumx.h"

#define MENU_WIDTH (LV_MIN(600, lv_obj_get_width(lv_scr_act()) * 2 / 3))
#define MENU_HEIGHT (LV_MIN(440, lv_obj_get_height(lv_scr_act()) * 2 / 3))

static void menu_pressed(lv_event_t *event)
{
    uint16_t row, col;
    lv_obj_t *menu = lv_event_get_target(event);
    menu_items_t *menu_items = menu->user_data;
    lv_table_get_selected_cell(menu, &row, &col);

    if (menu_items == NULL)
    {
        return;
    }

    if (menu_items[row].confirm_box != NULL)
    {
        confirmbox_open(menu_items[row].confirm_box, menu_items[row].cb, NULL);
    }
    else
    {
        if (menu_items[row].cb)
        {
            menu_items[row].cb(menu_items[row].callback_param);
        }
    }
}

static void menu_clean(lv_event_t *event)
{
    lv_obj_t *menu = lv_event_get_target(event);
    lv_mem_free(menu->user_data);
}

static void main_scroll(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);

    if (lv_obj_check_type(obj, &lv_table_class) == false)
    {
        return;
    }

    uint16_t row = 0, col = 0;
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

static void menu_close(lv_event_t *event)
{
    lv_obj_t *menu = lv_event_get_target(event);
    lv_key_t key = *((lv_key_t *)lv_event_get_param(event));

    if (key == LV_KEY_ESC)
    {
        lv_obj_t *window = lv_obj_get_parent(menu);
        lv_obj_del(window);
        dash_focus_pop_depth();
    }
}

static void menu_wrap(lv_event_t *event)
{
    lv_obj_t *menu = lv_event_get_target(event);
    lv_key_t key = *((lv_key_t *)lv_event_get_param(event));
    lv_table_t *t = (lv_table_t *)menu;

    // Jump to the start or end of menu if at each end
    if (key == LV_KEY_UP || key == LV_KEY_DOWN)
    {
        int prev_row = (intptr_t)lv_obj_get_parent(menu)->user_data;
        if (key == LV_KEY_UP)
        {
            if (t->row_act == 0 && prev_row == 0)
            {
                t->row_act = t->row_cnt - 1;
                lv_obj_invalidate(menu);
            }
        }

        else if (key == LV_KEY_DOWN)
        {
            if (t->row_act == (t->row_cnt - 1) && prev_row == (t->row_cnt - 1))
            {
                t->row_act = 0;
                lv_obj_invalidate(menu);
            }
        }
        lv_obj_get_parent(menu)->user_data = (void *)(intptr_t)t->row_act;
        main_scroll(event);
    }
}

static void main_refocus(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_set_style_bg_opa(lv_obj_get_parent(obj), LV_OPA_60, LV_PART_MAIN);
}

static void main_unfocus(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_set_style_bg_opa(lv_obj_get_parent(obj), LV_OPA_0, LV_PART_MAIN);
}

lv_obj_t *menu_open(menu_items_t *menu_items, int cnt)
{
    menu_items_t *i = lv_mem_alloc(sizeof(menu_items_t) * cnt);
    lv_memcpy(i, menu_items, sizeof(menu_items_t) * cnt);
    lv_obj_t *menu = menu_open_static(i, cnt);
    lv_obj_add_event_cb(menu, menu_clean, LV_EVENT_DELETE, NULL);
    return menu;
}

lv_obj_t *menu_open_static(const menu_items_t *menu_items, int cnt)
{
    lv_obj_t *window = lv_obj_create(lv_scr_act());
    lv_obj_set_size(window, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_add_style(window, &menu_table_style, LV_PART_MAIN);
    lv_obj_set_style_border_width(window, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(window, LV_OPA_0, LV_PART_MAIN);
    window->user_data = 0;

    lv_obj_t *menu;
    if (menu_items)
    {
        assert(cnt > 0);
        menu = lv_table_create(window);
        lv_table_set_col_cnt(menu, 1);
        lv_table_set_row_cnt(menu, cnt);
        lv_table_set_col_width(menu, 0, MENU_WIDTH);
        menu->user_data = (void *)menu_items;
        for (int i = 0; i < cnt; i++)
        {
            lv_table_add_cell_ctrl(menu, i, 0, LV_TABLE_CELL_CTRL_TEXT_CROP);
            lv_table_set_cell_value(menu, i, 0, menu_items[i].str);
        }
        lv_obj_update_layout(menu);

        lv_obj_add_event_cb(menu, menu_pressed, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(menu, menu_wrap, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(menu, main_scroll, LV_EVENT_VALUE_CHANGED, NULL);
        
    }
    else
    {
        menu = lv_obj_create(window);
    }

    lv_obj_add_event_cb(menu, menu_close, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(menu, main_unfocus, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(menu, main_refocus, LV_EVENT_FOCUSED, NULL);

    lv_obj_align(menu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(menu, MENU_WIDTH, LV_SIZE_CONTENT);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_max_height(menu, MENU_HEIGHT, LV_PART_MAIN);

    lv_obj_add_style(menu, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(menu, &menu_table_style, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(menu, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_add_style(menu, &menu_table_highlight_style, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    dash_focus_change_depth(menu);
    return menu;
}

void menu_force_value(lv_obj_t *menu, int row)
{
    assert(lv_obj_get_class(menu) == &lv_table_class);
    lv_table_t *t = (lv_table_t *)menu;
    if (row >= t->row_cnt)
    {
        row = t->row_cnt - 1;
    }
    t->row_act = row;
    lv_obj_get_parent(menu)->user_data = (void *)(intptr_t)t->row_act;
    lv_event_send(menu, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_invalidate(menu);
}
