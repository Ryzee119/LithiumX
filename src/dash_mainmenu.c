// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

static void dash_system_info(void *param);
static void dash_utilities(void *param);
static void dash_settings_page(void *param);
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
    strcpy(dash_launch_path, "__MSDASH__");
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
    strcpy(dash_launch_path, "__DVD__");
    lv_set_quit(LV_QUIT_OTHER);
}

static void dash_flush_cache(void *param)
{
    (void)param;
    platform_flush_cache();
}

static void item_launch_abort(lv_event_t *event)
{
    lv_mem_free(lv_event_get_user_data(event));
}

static void item_launch(void *param)
{
    dash_launcher_go(param);
}

static bool dash_browser_item_selection_cb(const char *selected_path)
{
    char cb_text[DASH_MAX_PATH];
    if (dash_launcher_is_launchable(selected_path))
    {
        lv_snprintf(cb_text, DASH_MAX_PATH, "Launch \"%s\"", selected_path);

        char *t = lv_mem_alloc(DASH_MAX_PATH);
        strncpy(t, selected_path, DASH_MAX_PATH - 1);
        lv_obj_t *cb = confirmbox_open(cb_text, item_launch, t);
        lv_obj_add_event_cb(cb, item_launch_abort, LV_EVENT_DELETE, t);
    }
    return false;
}

static void dash_open_xbe_launcher(void *param)
{
    (void)param;
    dash_browser_open(DASH_ROOT_PATH, dash_browser_item_selection_cb);
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
    // Set dash_settings.earliest_recent_date to now which effectively clears
    // recent items
    platform_get_iso8601_time(dash_settings.earliest_recent_date);
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

static void dash_settings_page(void *param)
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
            {"Settings", dash_settings_page, NULL, NULL},
            {"About", dash_open_about, NULL, NULL},
            {"Reboot", dash_reboot, NULL, "Accept \"Reboot\""},
            {"Shutdown", dash_shutdown, NULL, "Accept \"Shutdown\""},
        };
    lv_obj_t *menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    lv_obj_add_event_cb(menu, mainmenu_close, LV_EVENT_KEY, NULL);
}
