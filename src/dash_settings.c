// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

static const char *f_off = "Fahrenheit Display   OFF";
static const char *d_off = "Autolaunch DVD       OFF";
static const char *f_on = "Fahrenheit Display   ON";
static const char *d_on = "Autolaunch DVD       ON";

static int dash_settings_read_callback(void *param, int argc, char **argv, char **azColName)
{
    (void)param;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(azColName[i], SQL_SETTINGS_FAHRENHEIT) == 0)
        {
            settings_use_fahrenheit = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_AUTOLAUNCH_DVD) == 0)
        {
            settings_auto_launch_dvd = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_DEFAULT_PAGE_INDEX) == 0)
        {
            settings_default_page_index = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_THEME_COLOR) == 0)
        {
            settings_theme_colour = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_MAX_RECENT) == 0)
        {
            settings_max_recent = atoi(argv[i]);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_EARLIEST_RECENT_DATE) == 0)
        {
            strncpy(settings_earliest_recent_date, argv[i], sizeof(settings_earliest_recent_date) - 1);
        }
        else if (strcmp(azColName[i], SQL_SETTINGS_PAGE_SORTS) == 0)
        {
            int len = strlen(argv[i]);
            if (len)
            {
                strncpy(settings_page_sorts_str, argv[i], sizeof(settings_page_sorts_str) - 1);
            }
        }
    }
    return 0;
}

static void fahrenheit_change_callback(void *param)
{
    settings_use_fahrenheit = LV_MIN(1, settings_use_fahrenheit);
    settings_use_fahrenheit ^= 1;
    lv_table_set_cell_value(param, 0, 0, (settings_use_fahrenheit) ? f_on : f_off);
    dash_settings_apply();
}

static void autolaunch_dvd_change_callback(void *param)
{
    settings_auto_launch_dvd = LV_MIN(1, settings_auto_launch_dvd);
    settings_auto_launch_dvd ^= 1;
    lv_table_set_cell_value(param, 1, 0, (settings_auto_launch_dvd) ? d_on : d_off);
    dash_settings_apply();
}

static void change_page_sort_sub_submenu_callback(void *param)
{
    int values = (int)(intptr_t)param;
    int page_index = values >> 16;
    int sort_index = values & 0xFF;
    int current_sort_index;

    int cursor = 0;
    int max_len = DASH_MAX_PATH * dash_scroller_get_page_count();
    char *new_sorts = lv_mem_alloc(max_len);
    for (int i = 0; i < dash_scroller_get_page_count(); i++)
    {
        const char *page_title = dash_scroller_get_title(i);
        assert(page_title != NULL);
        // Dont want to sort the Recent Page
        if (strcmp(page_title, "Recent") == 0)
        {
            continue;
        }
        current_sort_index = 0;
        dash_scroller_get_sort_value(page_title, &current_sort_index);
        cursor += lv_snprintf(&new_sorts[cursor], max_len - cursor, "%s=%d ",
                              page_title, (i == page_index) ? sort_index : current_sort_index);
    }
    strncpy(settings_page_sorts_str, new_sorts, sizeof(settings_page_sorts_str) - 1);
    lv_mem_free(new_sorts);
    dash_settings_apply();
    dash_scroller_resort_page(dash_scroller_get_title(page_index));
}

static void startup_page_change_submenu_callback(void *param)
{
    int new_index = (int)((intptr_t)param);
    settings_default_page_index = new_index;
    dash_settings_apply();
}

static void change_startup_page_submenu(void *param)
{
    (void)param;

    int page_cnt = dash_scroller_get_page_count();
    menu_items_t items[page_cnt];
    for (int i = 0; i < page_cnt; i++)
    {
        items[i].cb = startup_page_change_submenu_callback;
        items[i].callback_param = (void *)((intptr_t)i);
        items[i].confirm_box = NULL;
        items[i].str = dash_scroller_get_title(i);
    }

    lv_obj_t *menu = menu_open(items, DASH_ARRAY_SIZE(items));
    menu_force_value(menu, settings_default_page_index);
}

static void change_page_sort_sub_submenu(void *param)
{
    (void)param;
    int page_index = (int)(intptr_t)param;
    int p = page_index << 16;

    // Instead of allocating memory for a parameter we pack in the sort index and page index into
    // the pointer
    menu_items_t items[] =
        {
            {"A-Z", change_page_sort_sub_submenu_callback, (void *)(intptr_t)(DASH_SORT_A_Z | p), NULL},
            {"Rating", change_page_sort_sub_submenu_callback, (void *)(intptr_t)(DASH_SORT_RATING | p), NULL},
            {"Last Launch", change_page_sort_sub_submenu_callback, (void *)(intptr_t)(DASH_SORT_LAST_LAUNCH | p), NULL},
            {"Release", change_page_sort_sub_submenu_callback, (void *)(intptr_t)(DASH_SORT_RELEASE_DATE | p), NULL},
        };

    lv_obj_t *menu = menu_open(items, DASH_ARRAY_SIZE(items));
    const char *page_title = dash_scroller_get_title(page_index);
    if (page_title)
    {
        int sort_value;
        if (dash_scroller_get_sort_value(page_title, &sort_value))
        {
            menu_force_value(menu, sort_value);
        }
    }
}

static void change_page_sort_submenu(void *param)
{
    (void)param;

    int page_cnt = dash_scroller_get_page_count();
    menu_items_t items[page_cnt];
    int rows = 0;
    for (int i = 0; i < page_cnt; i++)
    {
        // Ignore recent page as we dont want to sort that
        if (strcmp(dash_scroller_get_title(i), "Recent") == 0)
        {
            continue;
        }
        items[rows].cb = change_page_sort_sub_submenu;
        items[rows].callback_param = (void *)((intptr_t)i);
        items[rows].confirm_box = NULL;
        items[rows].str = dash_scroller_get_title(i);
        rows++;
    }

    menu_open(items, rows);
}

static void change_max_recent_submenu_cb(void *param)
{
    int n = (intptr_t)param;
    settings_max_recent = n;
    dash_settings_apply();
}

static void change_max_recent_submenu(void *param)
{
    (void) param;
    const int MAX_RECENT_ITEMS = 32;
    menu_items_t *items = lv_mem_alloc(sizeof(menu_items_t) * MAX_RECENT_ITEMS);
    for (int i = 0; i < MAX_RECENT_ITEMS; i++)
    {
        items[i].callback_param = (void *)(intptr_t)i + 1;
        items[i].cb = change_max_recent_submenu_cb;
        items[i].confirm_box = NULL;
        items[i].str = "";
    }
    lv_obj_t *number_list = menu_open(items, MAX_RECENT_ITEMS);
    for (int i = 0; i < MAX_RECENT_ITEMS; i++)
    {
        lv_table_set_cell_value_fmt(number_list, i, 0, "%d", i + 1);
    }
    settings_max_recent = LV_CLAMP(1, settings_max_recent, MAX_RECENT_ITEMS);
    menu_force_value(number_list, settings_max_recent - 1);
}

#define COLOR_HSV_MIN_V (1)
#define COLOR_TABLE_HUE_RESOLUTION (18)
#define COLOR_TABLE_VALUE_RESOLUTION (10)
#define COLOR_TABLE_HOR_CNT (360 / COLOR_TABLE_HUE_RESOLUTION)
#define COLOR_TABLE_VER_CNT ((100 / COLOR_TABLE_VALUE_RESOLUTION) - COLOR_HSV_MIN_V)
lv_color_t *hsv_colors;

static void get_grid_from_rgb(lv_color_t rgb, int *row, int *col)
{
    for (int i = 0; i < (COLOR_TABLE_HOR_CNT * COLOR_TABLE_VER_CNT); i++)
    {
        lv_color_t rgb_check = hsv_colors[i];
        if (memcmp(&rgb_check, &rgb, sizeof(lv_color_t)) == 0)
        {
            int c = i % COLOR_TABLE_HOR_CNT;
            int r = i / COLOR_TABLE_HOR_CNT;
            *row = r;
            *col = c;
            return;
        }
    }

    // Couldnt find exact match. Fall back to approximate
    lv_color_hsv_t hsv = lv_color_rgb_to_hsv(rgb.ch.red, rgb.ch.green, rgb.ch.blue);
    *col = hsv.h / COLOR_TABLE_HUE_RESOLUTION;
    *row = hsv.v / COLOR_TABLE_VALUE_RESOLUTION;
}

static void base_color_change_submenu_callback(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);
   
    lv_obj_t *color_rect = lv_event_get_user_data(event);
    lv_table_t *table = (lv_table_t *)target;

    lv_color_t c = hsv_colors[(table->row_act * table->col_cnt) + table->col_act];
    lv_obj_set_style_bg_color(color_rect, c, LV_PART_MAIN);

    settings_theme_colour = (c.ch.red << 16) | (c.ch.green << 8) | (c.ch.blue);
    dash_settings_apply();
}

static void base_color_draw_hsv(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    lv_obj_draw_part_dsc_t *pdsc = lv_event_get_param(e);
    lv_table_t *table = (lv_table_t *)lv_event_get_target(e);

    dsc->rect_dsc->bg_color = hsv_colors[pdsc->id];

    unsigned int id = (table->row_act * COLOR_TABLE_HOR_CNT) + table->col_act;
    if (pdsc->id == id)
    {
        dsc->rect_dsc->border_color = lv_color_white();
        dsc->rect_dsc->border_width = 1;
    }
}

static void hsv_table_clean(lv_event_t *e)
{
    (void) e;
    lv_mem_free(hsv_colors);
}

static void change_base_color_submenu(void *param)
{
    (void)param;
    lv_obj_t *c = container_open();

    // Create a grid for our colour picker
    lv_obj_t *hsv_table = lv_table_create(c);
    lv_obj_align(hsv_table, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(hsv_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(hsv_table, LV_OPA_50, LV_PART_MAIN);

    lv_table_set_col_cnt(hsv_table, COLOR_TABLE_HOR_CNT);
    lv_table_set_row_cnt(hsv_table, COLOR_TABLE_VER_CNT);
    lv_obj_set_style_min_height(hsv_table, 250 / COLOR_TABLE_VER_CNT, LV_PART_ITEMS);
    lv_obj_set_style_max_height(hsv_table, 250 / COLOR_TABLE_VER_CNT, LV_PART_ITEMS);
    for (int i = 0; i < COLOR_TABLE_HOR_CNT; i++)
    {
        lv_table_set_col_width(hsv_table, i, lv_obj_get_width(c) / COLOR_TABLE_HOR_CNT);
    }
    lv_obj_update_layout(hsv_table);

    hsv_colors = lv_mem_alloc(COLOR_TABLE_HOR_CNT * COLOR_TABLE_VER_CNT * sizeof(lv_color_t));
    for (int i = 0; i < (COLOR_TABLE_HOR_CNT * COLOR_TABLE_VER_CNT); i++)
    {
        int c = i % COLOR_TABLE_HOR_CNT;
        int r = i / COLOR_TABLE_HOR_CNT;
        lv_color_hsv_t hsv;
        hsv.s = 100;
        hsv.h = c * COLOR_TABLE_HUE_RESOLUTION;
        hsv.v = (r + COLOR_HSV_MIN_V) * COLOR_TABLE_VALUE_RESOLUTION;
        hsv_colors[i] = lv_color_hsv_to_rgb(hsv.h, hsv.s, hsv.v);
    }
    lv_obj_add_event_cb(hsv_table, base_color_draw_hsv, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(hsv_table, hsv_table_clean, LV_EVENT_DELETE, NULL);

    lv_color_t current_theme = lv_color_make(settings_theme_colour >> 16,
                                             settings_theme_colour >> 8,
                                             settings_theme_colour);

    // Set the currently selected box to the current theme color.
    int row,col;
    get_grid_from_rgb(current_theme, &row, &col);
    ((lv_table_t *)hsv_table)->row_act = row;
    ((lv_table_t *)hsv_table)->col_act = col;

    // Create a colored rectangle that displays the currently set color
    lv_obj_t *rect = lv_obj_create(c);
    lv_obj_add_style(rect, &object_style, LV_PART_MAIN);
    lv_obj_set_size(rect, 50, 50);
    lv_obj_align(rect, LV_ALIGN_TOP_MID, 0, lv_obj_get_y2(hsv_table) + 10);
    lv_obj_set_style_bg_color(rect, current_theme, LV_PART_MAIN);

    // Create a text box that shows some info
    lv_obj_t *label = lv_label_create(c);
    lv_label_set_text_static(label, "Applies on\nReboot");
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(hsv_table, base_color_change_submenu_callback, LV_EVENT_KEY, rect);
}

static void change_search_path_submenu(void *param)
{
    (void) param;
    lv_obj_t *c = container_open();
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(l, lv_obj_get_width(c), LV_SIZE_CONTENT);
    lv_label_set_text_fmt(l, "Not supported yet. To change folder search paths, edit \"%s\"."
                            " Then rebuild database", DASH_SEARCH_PATH_CONFIG);
}

void dash_settings_open()
{
    static menu_items_t items[] =
        {
            {"", fahrenheit_change_callback, NULL, NULL},
            {"", autolaunch_dvd_change_callback, NULL, NULL},
            {"Change How Pages are Sorted", change_page_sort_submenu, NULL, NULL},
            {"Change Max Recent Items Shown", change_max_recent_submenu, NULL, NULL},
            {"Change Default Start Up Page", change_startup_page_submenu, NULL, NULL},
            {"Change Base Theme Color", change_base_color_submenu, NULL, NULL},
            {"Change Folder Search Paths", change_search_path_submenu, NULL, NULL},
        };

    items[0].str = (settings_use_fahrenheit) ? f_on : f_off;
    items[1].str = (settings_auto_launch_dvd) ? d_on : d_off;
    lv_obj_t *menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    for (unsigned int i = 0; i < DASH_ARRAY_SIZE(items); i++)
    {
        items[i].callback_param = menu;
    }
}

void dash_settings_read()
{
    db_command_with_callback(SQL_SETTINGS_READ, dash_settings_read_callback, NULL);
}

void dash_settings_apply()
{
    db_command_with_callback(SQL_SETTINGS_DELETE_ENTRIES, NULL, NULL);

    char settings_use_fahrenheit_str[2];
    char settings_auto_launch_dvd_str[2];
    char settings_default_page_index_str[3];
    char settings_theme_colour_str[11];
    char settings_max_recent_str[4];

    lv_snprintf(settings_use_fahrenheit_str, sizeof(settings_use_fahrenheit_str), "%d", LV_MIN(1, settings_use_fahrenheit));
    lv_snprintf(settings_auto_launch_dvd_str, sizeof(settings_auto_launch_dvd_str), "%d", LV_MIN(1, settings_auto_launch_dvd));
    lv_snprintf(settings_default_page_index_str, sizeof(settings_default_page_index_str), "%d", settings_default_page_index);
    lv_snprintf(settings_theme_colour_str, sizeof(settings_theme_colour_str), "%d", settings_theme_colour);
    lv_snprintf(settings_max_recent_str, sizeof(settings_max_recent_str), "%d", settings_max_recent);

    db_insert(SQL_SETTINGS_INSERT, SQL_SETTINGS_INSERT_CNT, SQL_SETTINGS_INSERT_FORMAT,
            settings_use_fahrenheit_str,
            settings_auto_launch_dvd_str,
            settings_default_page_index_str,
            settings_theme_colour_str,
            settings_earliest_recent_date,
            settings_page_sorts_str,
            settings_max_recent_str);
}
