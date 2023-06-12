// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

static void dash_system_info(void *param);
static void dash_utilities(void *param);
static void dash_settings(void *param);
static void dash_launch_msdash(void *param);
static void dash_launch_dvd(void *param);
static void dash_flush_cache(void *param);
static void dash_open_xbe_launcher(void *param);
static void dash_open_eeprom_config(void *param);
static void dash_open_about(void *param);
static void dash_reboot(void *param);
static void dash_shutdown(void *param);

static void dash_system_info(void *param)
{
    (void)param;
    lv_obj_t *window = container_open();
    platform_system_info(window);
}

static void dash_launch_msdash(void *param)
{
    (void)param;
    dash_launch_path = "__MSDASH__";
    lv_set_quit(LV_QUIT_OTHER);
}

static void dash_launch_dvd(void *param)
{
    #ifdef NXDK
    // Only launch DVD if media (Xbox Game) is detected
    ULONG tray_state = 0x70;
    NTSTATUS status = HalReadSMCTrayState(&tray_state, NULL);
    if (!NT_SUCCESS(status) || tray_state != 0x60)
    {
        return;
    }
    #endif
    (void)param;
    dash_launch_path = "__DVD__";
    lv_set_quit(LV_QUIT_OTHER);
}

static void dash_flush_cache(void *param)
{
    (void)param;
    platform_flush_cache();
}

static char *last_backslash(const char *str)
{
    char *lastBackslash = NULL;
    char *current = strchr(str, '\\');
    while (current != NULL)
    {
        lastBackslash = current;
        current = strchr(current + 1, '\\');
    }
    return lastBackslash;
}

static bool is_xbe(const char *file_path, char *title, char *title_id)
{
    const int extlen = 4; //".xbe"
    int slen = strlen(file_path);
    if (slen > extlen)
    {
        const char *ext = file_path + slen - extlen;
        if (strcasecmp(ext, ".xbe") == 0)
        {
            bool ret = false;
            char *folder_path = lv_mem_alloc(strlen(file_path) + 1);
            strcpy(folder_path, file_path);
            char *last = last_backslash(folder_path);
            if (last != NULL)
            {
                last[0] = '\0'; // Split it
                ret = db_xbe_parse(file_path, folder_path, title, title_id);
            }
            lv_mem_free(folder_path);
            return ret;
        }
    }
    return false;
}

static int recent_title_exists_cb(void *param, int argc, char **argv, char **azColName)
{
    (void)argc; (void)argv; (void)azColName;
    int *db_id = param;
    *db_id = atoi(argv[0]);
    return 0;
}

static int recent_title_get_last_id_cb(void *param, int argc, char **argv, char **azColName)
{
    (void)argc; (void)argv; (void)azColName;
    int *db_id_max = param;
    if (argv[0] != NULL)
        *db_id_max = atoi(argv[0]);
    return 0;
}

typedef struct xbe_launch_param
{
    char title[MAX_META_LEN];
    char title_id[MAX_META_LEN];
    char selected_path[DASH_MAX_PATH];
} xbe_launch_param_t;

static void xbe_launch_abort(lv_event_t *event)
{
    lv_mem_free(lv_event_get_user_data(event));
}

static void xbe_launch(void *param)
{
    static const char *no_meta = "No Meta-Data";
    char cmd[SQL_MAX_COMMAND_LEN];
    char time_str[20];
    xbe_launch_param_t *xbe_params = param;
    platform_get_iso8601_time(time_str);

    // See if the launch paths exists in page "Recent"
    const char *query = "SELECT " SQL_TITLE_DB_ID " FROM " SQL_TITLES_NAME
                        " WHERE " SQL_TITLE_LAUNCH_PATH "= \"%s\" AND " SQL_TITLE_PAGE " = \"__RECENT__\"";
    lv_snprintf(cmd, sizeof(cmd), query, xbe_params->selected_path);
    int db_id = -1;
    db_command_with_callback(cmd, recent_title_exists_cb, &db_id);
    if (db_id >= 0)
    {
        // If it does, update the LAUNCH_DATETIME to now
        lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_SET_LAST_LAUNCH_DATETIME, time_str, db_id);
        db_command_with_callback(cmd, NULL, NULL);
    }
    else
    {
        // Otherwise add it to a page called "Recent" with current LAUNCH_DATETIME
        const char *query = "SELECT MAX(" SQL_TITLE_DB_ID ") FROM " SQL_TITLES_NAME
                            " WHERE " SQL_TITLE_PAGE " = \"__RECENT__\"";
        int db_id_max = 10000;
        db_command_with_callback(query, recent_title_get_last_id_cb, &db_id_max);
        db_id_max++;
        db_id_max = LV_MAX(10000, db_id_max);

        char item_index_str[8];
        lv_snprintf(item_index_str, sizeof(item_index_str), "%d", db_id_max);
        db_insert(SQL_TITLE_INSERT, SQL_TITLE_INSERT_CNT, SQL_TITLE_INSERT_FORMAT,
            item_index_str,
            xbe_params->title_id,
            xbe_params->title,
            xbe_params->selected_path,
            "__RECENT__",
            no_meta,
            no_meta,
            no_meta,
            no_meta,
            time_str,
            "0.0");
    }

    // Setup launch path then quit
    dash_launch_path = xbe_params->selected_path;
    lv_set_quit(LV_QUIT_OTHER);
}

static bool xbe_selection_cb(const char *selected_path)
{
    char title[MAX_META_LEN];
    char title_id[MAX_META_LEN];
    if (is_xbe(selected_path, title, title_id))
    {
        xbe_launch_param_t *xbe_params = lv_mem_alloc(sizeof(xbe_launch_param_t));
        strcpy(xbe_params->selected_path, selected_path);
        strcpy(xbe_params->title, title);
        strcpy(xbe_params->title_id, title_id);
        char cb_text[DASH_MAX_PATH];
        lv_snprintf(cb_text, DASH_MAX_PATH, "Launch \"%s\"", selected_path);
        lv_obj_t *cb = confirmbox_open(cb_text, xbe_launch, xbe_params);
        lv_obj_add_event_cb(cb, xbe_launch_abort, LV_EVENT_DELETE, xbe_params);
    }
    return false;
}

static void dash_open_xbe_launcher(void *param)
{
    (void)param;
    dash_browser_open(DASH_ROOT_PATH, xbe_selection_cb);
}

static void dash_open_eeprom_config(void *param)
{
    (void)param;
    dash_eeprom_settings_open();
}

static void dash_rebuild_database(void *param)
{
    (void)param;
    db_command_with_callback(SQL_TITLE_DELETE_ENTRIES, NULL, NULL);
}

static void dash_clear_recent(void *param)
{
    (void)param;
    // Set settings_earliest_recent_date to now which effectively clears
    // recent items
    platform_get_iso8601_time(settings_earliest_recent_date);
    dash_settings_apply(false);

    static const char *cmd = "DELETE FROM " SQL_TITLES_NAME " WHERE page = \"__RECENT__\"";
    db_command_with_callback(cmd, NULL, NULL);
    dash_scroller_clear_page("Recent");
}

static void dash_open_about(void *param)
{
    (void)param;
    const char *url = "https://github.com/Ryzee119/LithiumX";

    lv_obj_t *window = container_open();

    lv_obj_t *qr = lv_qrcode_create(window, 256, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, url, strlen(url));
    lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_update_layout(window);

    lv_obj_t *label = lv_label_create(window);
    lv_label_set_text(label, "See github.com/Ryzee119/LithiumX");
    lv_obj_update_layout(label);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_set_height(window, lv_obj_get_height(qr) + lv_obj_get_height(label) + 2);
    lv_obj_update_layout(window);
}

static void dash_reboot(void *param)
{
    (void)param;
    lv_set_quit(LV_REBOOT);
}

static void dash_shutdown(void *param)
{
    (void)param;
    lv_set_quit(LV_SHUTDOWN);
}

static void dash_settings(void *param)
{
    (void)param;
    dash_settings_open();
}

static void dash_utilities(void *param)
{
    (void)param;
    static const menu_items_t items[] =
        {
            {"XBE Launcher", dash_open_xbe_launcher, NULL, NULL},
            {"EEPROM Config", dash_open_eeprom_config, NULL, NULL},
            {"Clear Recent Titles", dash_clear_recent, NULL, "Accept \"Clear Recent Titles\""},
            {"Flush Cache Partitions", dash_flush_cache, NULL, "Accept \"Flush Cache Partitions\""},
            {"Mark Database Reset at Reboot", dash_rebuild_database, NULL, "Accept \"Database Reset\""},
        };
    menu_open_static(items, DASH_ARRAY_SIZE(items));
}

static void mainmenu_close(lv_event_t *event)
{
    lv_obj_t *menu = lv_event_get_target(event);
    lv_key_t key = *((lv_key_t *)lv_event_get_param(event));
    if (key == DASH_SETTINGS_PAGE)
    {
        static int key = LV_KEY_ESC;
        lv_event_send(menu, LV_EVENT_KEY, &key);
    }
}

void dash_mainmenu_open()
{
    static const menu_items_t items[] =
        {
            {"System Information", dash_system_info, NULL, NULL},
            {"Launch MS Dashboard", dash_launch_msdash, NULL, "Accept \"Launch MS Dashboard\""},
            {"Launch DVD", dash_launch_dvd, NULL, "Accept \"Launch DVD\""},
            {"Utilities", dash_utilities, NULL, NULL},
            {"Settings", dash_settings, NULL, NULL},
            {"About", dash_open_about, NULL, NULL},
            {"Reboot", dash_reboot, NULL, "Accept \"Reboot\""},
            {"Shutdown", dash_shutdown, NULL, "Accept \"Shutdown\""},
        };
    lv_obj_t *menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    lv_obj_add_event_cb(menu, mainmenu_close, LV_EVENT_KEY, NULL);
}
