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
#define MAINMENU_HEIGHT (lv_obj_get_height(lv_scr_act()) - (2 * DASH_YMARGIN))

static lv_obj_t *main_menu;
static lv_obj_t *rt_info;
static lv_timer_t *rt_info_timer;

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
static void dash_shutdown()
{
    lv_set_quit(LV_SHUTDOWN);
}

static void dash_reboot()
{
    lv_set_quit(LV_REBOOT);
}

static void dash_clear_recent()
{
    dash_clear_recent_list();
}

#ifdef NXDK
static void dash_launch_msdash()
{
    dash_set_launch_folder("MSDASH");
    lv_set_quit(LV_QUIT_OTHER);
}

static void dash_launch_dvd()
{
    platform_launch_dvd();
}

static void dash_flush_cache()
{
    platform_flush_cache_cb();
}
#endif

static void realtime_info_cb(lv_timer_t *event)
{
    lv_obj_t *rt_info = event->user_data;
    if (lv_obj_is_valid(rt_info) == false)
    {
        lv_timer_del(event);
        return;
    }
    const char *rt_info_str = platform_realtime_info_cb();
    lv_label_set_text(rt_info, rt_info_str);
    lv_obj_update_layout(rt_info);

    //If there's alot of realtime text, it may take up multiple lines. We need to shrink the menu size
    int max_height = lv_obj_get_height(lv_scr_act()) - 2 * DASH_YMARGIN - lv_obj_get_height(rt_info);
    if (lv_obj_get_height(main_menu) > max_height)
    {
        lv_obj_set_height(main_menu, max_height);
        lv_obj_update_layout(main_menu);
    }

    lv_obj_align(rt_info, LV_ALIGN_CENTER, 0,
                 (-lv_obj_get_height(main_menu) - lv_obj_get_height(rt_info)) / 2);

}

// Menu or submenu close callback on ESC or DASH_SETTINGS_PAGE (B or BACK) to close
static void close_callback(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_group_t *gp = lv_group_get_default();
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    menu_data_t *menu_data = (menu_data_t *)obj->user_data;
    if (key == LV_KEY_ESC || key == DASH_SETTINGS_PAGE)
    {
        if (obj == main_menu)
        {
            lv_obj_del(rt_info);
            lv_timer_del(rt_info_timer);
            //Main menu is about to be deleted, invalidate this pointer.
            main_menu = NULL;
        }
        lv_group_focus_freeze(gp, false);
        // If the object was on the recent items page, it may have been cleared. Ensure its valid still
        // before refocusing on it
        if (lv_obj_is_valid(menu_data->old_focus))
        {
            lv_group_focus_obj(menu_data->old_focus);
        }
        else if (lv_obj_is_valid(menu_data->old_focus_parent))
        {
            lv_group_focus_obj(menu_data->old_focus_parent);
        }
        lv_mem_free(menu_data);
        lv_obj_del(obj);
    }
}

//Scroll the main menu table if the selected cell is outside of the container view
static void scroll_callback(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
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
    lv_group_t *gp = lv_group_get_default();
    menu_data_t *menu_data = (menu_data_t *)obj->user_data;
    lv_table_get_selected_cell(obj, &row, &col);
    if (row == SUBMENU_ACCEPT)
    {
        if (menu_data->cb)
        {
            menu_data->cb();
        }
    }
    // Reselect the item that was previously selected before the menu was opened
    lv_group_focus_freeze(gp, false);
    lv_group_focus_obj(menu_data->old_focus);
    lv_mem_free(menu_data);
    lv_obj_del(obj);
}

// Helper function to apply generic style and focus rules to menu and submenu containers
static void menu_set_style(lv_obj_t *obj)
{
    lv_group_t *gp = lv_group_get_default();
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN);
    lv_group_add_obj(gp, obj);
    lv_group_focus_freeze(gp, false);
    lv_group_focus_obj(obj);
    lv_group_focus_freeze(gp, true);
}

// Create a generic submenu container
static lv_obj_t *menu_create_submenu_box()
{
    lv_obj_t *obj = lv_obj_create(lv_scr_act());
    lv_group_t *gp = lv_group_get_default();
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_set_width(obj, MAINMENU_WIDTH);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, confirmbox_callback, LV_EVENT_PRESSED, NULL);

    menu_data_t *user_data = lv_mem_alloc(sizeof(menu_data_t));
    user_data->old_focus = lv_group_get_focused(gp);
    user_data->old_focus_parent = lv_obj_get_parent(user_data->old_focus);
    user_data->cb = NULL;
    obj->user_data = user_data;

    menu_set_style(obj);
    lv_obj_add_event_cb(obj, close_callback, LV_EVENT_KEY, NULL);
    return obj;
}

// Callback if a button is actived on the main menu
static void mainmenu_callback(lv_event_t *e)
{
    char temp[DASH_MAX_PATHLEN];
    uint16_t row, col;
    lv_obj_t *submenu;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_table_get_selected_cell(obj, &row, &col);

    if (row == LV_TABLE_CELL_NONE)
    {
        return;
    }

    if (row == MENU_SYSTEM_INFO)
    {
        submenu = menu_create_submenu_box();
        lv_obj_set_layout(submenu, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(submenu, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_height(submenu, MAINMENU_HEIGHT);
        lv_obj_t *label;
        label = lv_label_create(submenu);
        lv_label_set_text(label, menu_items[row]);
        platform_show_info_cb(submenu);
    }
    else if (row == MENU_CLEAR_RECENT)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_clear_recent);
    }
#ifdef NXDK
    else if (row == MENU_LAUNCH_MS_DASH)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_launch_msdash);
    }
    else if (row == MENU_LAUNCH_DVD)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_launch_dvd);
    }
    else if (row == MENU_FLUSH_CACHE_PARTITION)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_flush_cache);
    }
#endif
    // About
    else if (row == MENU_ABOUT)
    {
        const char *url = QRURL;
        submenu = menu_create_submenu_box();
        lv_obj_align(submenu, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_width(submenu, lv_obj_get_width(lv_scr_act()));
        lv_obj_set_height(submenu, lv_obj_get_height(lv_scr_act()));

        lv_obj_t *qrcode = lv_qrcode_create(submenu, LV_MIN((MAINMENU_WIDTH * 3) / 4, (MAINMENU_HEIGHT * 3) / 4),
                                            lv_color_make(0x00, 0x00, 0x00),
                                            lv_color_make(0xFF, 0xFF, 0xFF));
        lv_obj_align(qrcode, LV_ALIGN_CENTER, 0, 0);
        lv_qrcode_update(qrcode, url, strlen(url));
        lv_obj_update_layout(qrcode);
        lv_obj_t *label = lv_label_create(submenu);
        lv_label_set_text_fmt(label, "Visit the Github Repo for info\n%s", url);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_update_layout(label);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -(lv_obj_get_height(qrcode) + lv_obj_get_height(label)) / 2 - 10);
    }
    else if (row == MENU_REBOOT)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_reboot);
    }
    else if (row == MENU_SHUTDOWN)
    {
        lv_snprintf(temp, sizeof(temp), "%s \"%s\"", "Confirm", menu_items[row]);
        submenu = menu_create_confirm_box(temp, dash_shutdown);
    }
}

void main_menu_init()
{
}

void main_menu_deinit(void)
{
}

// Create and open the main menu
void main_menu_open()
{
    nano_debug(LEVEL_TRACE, "TRACE: Opening main menu\n");
    lv_group_t *gp;

    gp = lv_group_get_default();
    main_menu = lv_table_create(lv_scr_act());
    menu_data_t *user_data = lv_mem_alloc(sizeof(menu_data_t));
    user_data->old_focus = lv_group_get_focused(gp);
    user_data->old_focus_parent = lv_obj_get_parent(user_data->old_focus);
    user_data->cb = NULL;
    main_menu->user_data = user_data;
    lv_obj_add_style(main_menu, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(main_menu, &menu_table_style, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_style(main_menu, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_add_style(main_menu, &menu_table_highlight_style, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_scrollbar_mode(main_menu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_width(main_menu, MAINMENU_WIDTH);
    lv_obj_align(main_menu, LV_ALIGN_CENTER, 0, 0);

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

    lv_obj_add_event_cb(main_menu, mainmenu_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(main_menu, close_callback, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(main_menu, scroll_callback, LV_EVENT_VALUE_CHANGED, NULL);

    lv_group_focus_obj(main_menu);
    lv_group_focus_freeze(gp, true);

    //Create a header above the menu to display realtime system info
    rt_info = lv_label_create(lv_scr_act());
    lv_obj_add_style(rt_info, &rtinfo_style, LV_PART_MAIN);
    lv_obj_set_width(rt_info, MAINMENU_WIDTH);
    lv_obj_set_style_text_align(rt_info, LV_TEXT_ALIGN_LEFT, 0);
    rt_info_timer = lv_timer_create(realtime_info_cb, 2000, rt_info);
    lv_timer_ready(rt_info_timer);
}

// Create a generic confirmation box. Options will be "Cancel" or Confirm "msg".
// If confirmed the confirm_cb will be called
lv_obj_t *menu_create_confirm_box(const char *msg, confirm_cb_t confirm_cb)
{
    lv_obj_t *obj = lv_table_create(lv_scr_act());
    lv_table_set_cell_value(obj, 0, 0, "Cancel");
    lv_table_set_cell_value(obj, 1, 0, msg);
    lv_table_set_col_width(obj, 0, MAINMENU_WIDTH);

    lv_group_t *gp = lv_group_get_default();
    lv_obj_add_style(obj, &menu_table_style, LV_PART_MAIN);
    lv_obj_add_style(obj, &menu_table_cell_style, LV_PART_ITEMS);
    lv_obj_set_width(obj, MAINMENU_WIDTH);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, confirmbox_callback, LV_EVENT_PRESSED, NULL);

    // We need to manually control the focus here so lvgl doesnt just to random places
    menu_data_t *user_data = lv_mem_alloc(sizeof(menu_data_t));
    user_data->old_focus = lv_group_get_focused(gp);
    user_data->old_focus_parent = lv_obj_get_parent(user_data->old_focus);
    user_data->cb = confirm_cb;
    obj->user_data = user_data;

    menu_set_style(obj);
    lv_obj_add_event_cb(obj, close_callback, LV_EVENT_KEY, NULL);
    return obj;
}
