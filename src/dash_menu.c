// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"
#include "dash_menu.h"
#include "platform/platform.h"
#include "helpers/nano_debug.h"
#include <stdlib.h>
#include <stdio.h>

#define MAINMENU_WIDTH 600
#define MAINMENU_HEIGHT 440

LV_IMG_DECLARE(qrcode);

static lv_obj_t *main_menu = NULL;
static lv_obj_t *sub_menu_container = NULL;
static lv_obj_t *confirm_box = NULL;
static lv_timer_t *realtime_info = NULL;

typedef struct
{
    lv_obj_t *old_focus_parent;
    lv_obj_t *old_focus;
    confirm_cb_t cb;
} menu_data_t;

enum
{
    MENU_SYSTEM_INFO,
    MENU_CLEAR_RECENT,
#ifdef NXDK
    MENU_LAUNCH_MS_DASH,
    MENU_LAUNCH_DVD,
    MENU_FLUSH_CACHE_PARTITION,
#endif
    MENU_ABOUT,
    MENU_REBOOT,
    MENU_SHUTDOWN,
    MENU_MAX,
};

static const char *menu_items[] =
    {
        "System Information",
        "Clear Recent Titles",
#ifdef NXDK
        "Launch MS Dashboard",
        "Launch DVD",
        "Flush Cache Partitions",
#endif
        "About",
        "Reboot",
        "Shutdown",
};

enum
{
    SUBMENU_CANCEL,
    SUBMENU_ACCEPT,
};

// The follow callbacks are called after the confirmation box has been accepted
static void dash_shutdown(void)
{
    lv_set_quit(LV_SHUTDOWN);
}

static void dash_reboot(void)
{
    lv_set_quit(LV_REBOOT);
}

static void dash_clear_recent(void)
{
    dash_clear_recent_list();
}

#ifdef NXDK
static void dash_launch_msdash(void)
{
    dash_set_launch_folder("MSDASH");
    lv_set_quit(LV_QUIT_OTHER);
}

static void dash_launch_dvd(void)
{
    platform_launch_dvd();
}

static void dash_flush_cache(void)
{
    platform_flush_cache_cb();
}
#endif

static void show_item(lv_obj_t *obj, confirm_cb_t confirmbox_cb)
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

static void hide_item(lv_obj_t *obj)
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

    if (realtime_info != NULL)
    {
        lv_timer_del(realtime_info);
        realtime_info = NULL;
    }
}

// Menu or submenu close callback on ESC or DASH_SETTINGS_PAGE (B or BACK) to close
static void menu_close(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_ESC || key == DASH_SETTINGS_PAGE)
    {
        hide_item(obj);
    }
}

// Basic containers dont scroll. We registered a custom scroll callback for text boxes. This is handled here.
static void text_scroll(lv_event_t *e)
{
    // objects normally scroll automatically however core object containers dont have any animations.
    // I replace the scrolling with my own here with animation enabled.
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_UP)
    {
        lv_obj_scroll_to_y(obj, lv_obj_get_scroll_y(obj) - lv_obj_get_height(obj) / 4, LV_ANIM_ON);
    }
    if (key == LV_KEY_DOWN)
    {
        lv_obj_scroll_to_y(obj, lv_obj_get_scroll_y(obj) + lv_obj_get_height(obj) / 4, LV_ANIM_ON);
    }
}

// Scroll the main menu table if the selected cell is outside of the container view
static void table_scroll(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint16_t row, col;
    lv_table_get_selected_cell(obj, &row, &col);
    int row_height = lv_font_get_line_height(lv_obj_get_style_text_font(obj, LV_PART_ITEMS)) +
                     lv_obj_get_style_pad_top(obj, LV_PART_ITEMS) +
                     lv_obj_get_style_pad_bottom(obj, LV_PART_ITEMS);
    int y = lv_obj_get_y(obj) + (row + 1) * row_height;
    int y2 = lv_obj_get_y2(obj);
    if (y > y2 || y < y2)
    {
        lv_obj_scroll_by_bounded(obj, 0, y2 - y, LV_ANIM_ON);
    }
}

// Callback when submenu/confirmbox has an item selected (Either cancel or accept action)
static void confirmbox_callback(lv_event_t *e)
{
    uint16_t row, col;
    lv_obj_t *obj = lv_event_get_target(e);
    menu_data_t *user_data = (menu_data_t *)obj->user_data;
    lv_table_get_selected_cell(obj, &row, &col);
    if (row == SUBMENU_ACCEPT && user_data->cb)
    {
        user_data->cb();
    }
    // Reselect the item that was previously selected before the menu was opened
    hide_item(obj);
}

// Periodic callback to update realtime info text
static void realtime_info_cb(lv_timer_t *t)
{
    lv_obj_t *label = t->user_data;
    if (lv_obj_is_valid(label))
    {
        lv_label_set_text(label, platform_realtime_info_cb());
    }
}

// Callback if a button is actived on the main menu
static void menu_pressed(lv_event_t *e)
{
    uint16_t row, col;
    lv_obj_t *obj = lv_event_get_target(e);

    lv_table_get_selected_cell(obj, &row, &col);

    if (row == LV_TABLE_CELL_NONE)
    {
        return;
    }

    // Prep the confirmation box text if used
    lv_table_set_cell_value_fmt(confirm_box, 1, 0, "%s \"%s\"", "Confirm", menu_items[row]);

    if (row == MENU_SYSTEM_INFO)
    {
        lv_obj_t *label;
        lv_obj_clean(sub_menu_container);
        lv_obj_set_layout(sub_menu_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(sub_menu_container, LV_FLEX_FLOW_COLUMN);

        label = lv_label_create(sub_menu_container);
        lv_label_set_text(label, menu_items[row]);

        label = lv_label_create(sub_menu_container);
        lv_label_set_recolor(label, true);
        realtime_info = lv_timer_create(realtime_info_cb, 1000, label);
        lv_timer_ready(realtime_info);

        label = lv_label_create(sub_menu_container);
        lv_label_set_recolor(label, true);
        lv_label_set_text(label, platform_show_info_cb());

        lv_obj_set_height(sub_menu_container, MAINMENU_HEIGHT);
        lv_obj_set_width(sub_menu_container, MAINMENU_WIDTH);

        show_item(sub_menu_container, NULL);
    }
    else if (row == MENU_ABOUT)
    {
        lv_obj_t *qrcode_img, *label;
        lv_obj_clean(sub_menu_container);
        lv_obj_set_layout(sub_menu_container, 0);

        lv_obj_set_width(sub_menu_container, lv_obj_get_width(lv_scr_act()));
        lv_obj_set_height(sub_menu_container, lv_obj_get_height(lv_scr_act()));

        lv_obj_clear_flag(sub_menu_container, LV_OBJ_FLAG_SCROLLABLE);             // Use my own scroll callback
        lv_obj_set_scroll_dir(sub_menu_container, LV_DIR_TOP);                     // Only scroll up and down

        qrcode_img = lv_img_create(sub_menu_container);
        lv_img_set_src(qrcode_img, &qrcode);
        lv_img_set_size_mode(qrcode_img, LV_IMG_SIZE_MODE_REAL);
        lv_img_set_antialias(qrcode_img, 0);
        lv_img_set_zoom(qrcode_img, 2048);
        lv_obj_align(qrcode_img, LV_ALIGN_CENTER, 0, 0);
        lv_obj_update_layout(qrcode_img);

        label = lv_label_create(sub_menu_container);
        lv_label_set_text_fmt(label, "Visit the Github Repo for info\n%s", QRURL);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_update_layout(label);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -(lv_obj_get_height(qrcode_img) + lv_obj_get_height(label)) / 2 - 10);

        show_item(sub_menu_container, NULL);
    }
    else if (row == MENU_CLEAR_RECENT)
    {
        show_item(confirm_box, dash_clear_recent);
    }
#ifdef NXDK
    else if (row == MENU_LAUNCH_MS_DASH)
    {
        show_item(confirm_box, dash_launch_msdash);
    }
    else if (row == MENU_LAUNCH_DVD)
    {
        show_item(confirm_box, dash_launch_dvd);
    }
    else if (row == MENU_FLUSH_CACHE_PARTITION)
    {
        show_item(confirm_box, dash_flush_cache);
    }
#endif
    else if (row == MENU_REBOOT)
    {
        show_item(confirm_box, dash_reboot);
    }
    else if (row == MENU_SHUTDOWN)
    {
        show_item(confirm_box, dash_shutdown);
    }
}

static void apply_style(lv_obj_t *obj)
{
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(obj, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_add_style(obj, &menu_table_highlight_style, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_width(obj, MAINMENU_WIDTH);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void main_menu_init(void)
{
    menu_data_t *user_data;
    lv_group_t *gp = lv_group_get_default();

    // Create a table for the main menu items
    main_menu = lv_table_create(lv_scr_act());
    user_data = (menu_data_t *)lv_mem_alloc(sizeof(menu_data_t));
    lv_memset(user_data, 0, sizeof(menu_data_t));
    main_menu->user_data = user_data;
    apply_style(main_menu);
    lv_obj_set_scrollbar_mode(main_menu, LV_SCROLLBAR_MODE_OFF);

    int row_cnt = sizeof(menu_items) / sizeof(menu_items[0]);
    lv_table_set_col_cnt(main_menu, 1);
    lv_table_set_row_cnt(main_menu, row_cnt);
    lv_table_set_col_width(main_menu, 0, MAINMENU_WIDTH);
    for (int i = 0; i < row_cnt; i++)
    {
        lv_table_set_cell_value(main_menu, i, 0, menu_items[i]);
    }
    lv_obj_set_height(main_menu, LV_SIZE_CONTENT);
    lv_obj_update_layout(main_menu);

    lv_obj_add_event_cb(main_menu, menu_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(main_menu, menu_close, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(main_menu, table_scroll, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(gp, main_menu);
    lv_obj_add_flag(main_menu, LV_OBJ_FLAG_HIDDEN);

    // Create a generator submenu container
    sub_menu_container = lv_obj_create(lv_scr_act());
    user_data = (menu_data_t *)lv_mem_alloc(sizeof(menu_data_t));
    lv_memset(user_data, 0, sizeof(menu_data_t));
    sub_menu_container->user_data = user_data;
    apply_style(sub_menu_container);
    lv_obj_add_event_cb(sub_menu_container, menu_close, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(sub_menu_container, text_scroll, LV_EVENT_KEY, NULL);
    lv_group_add_obj(gp, sub_menu_container);
    lv_obj_add_flag(sub_menu_container, LV_OBJ_FLAG_HIDDEN);

    // Create a table for the confirmation box
    confirm_box = lv_table_create(lv_scr_act());
    user_data = (menu_data_t *)lv_mem_alloc(sizeof(menu_data_t));
    lv_memset(user_data, 0, sizeof(menu_data_t));
    confirm_box->user_data = user_data;
    apply_style(confirm_box);
    lv_table_set_cell_value(confirm_box, 0, 0, "Cancel");
    lv_table_set_col_width(confirm_box, 0, MAINMENU_WIDTH);
    lv_obj_add_event_cb(confirm_box, confirmbox_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(confirm_box, menu_close, LV_EVENT_KEY, NULL);
    lv_group_add_obj(gp, confirm_box);
    lv_obj_add_flag(confirm_box, LV_OBJ_FLAG_HIDDEN);
}

void main_menu_deinit(void)
{
    lv_obj_del(main_menu);
    lv_obj_del(sub_menu_container);
    lv_obj_del(confirm_box);
    main_menu = NULL;
}

// Create and open the main menu
void main_menu_open(void)
{
    nano_debug(LEVEL_TRACE, "TRACE: Opening main menu\n");
    show_item(main_menu, NULL);
}

void confirmbox_open(const char *msg, confirm_cb_t confirmbox_cb)
{
    lv_table_set_cell_value_fmt(confirm_box, 1, 0, msg);
    show_item(confirm_box, confirmbox_cb);
}
