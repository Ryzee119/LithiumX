// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

static int synop_info_callback(void *param, int argc, char **argv, char **azColName)
{
    (void) azColName;
    (void) argc;

    lv_obj_t *synop_text = param;
    const char *format = "%s Title:# %s\n"
                         "%s Developer:# %s\n"
                         "%s Publisher:# %s\n"
                         "%s Release Date:# %s\n"
                         "%s Rating:# %s/10\n"
                         "%s Overview:# %s";
    assert(argc == DB_INDEX_MAX);
    assert(strcmp(azColName[DB_INDEX_TITLE], "title") == 0);
    assert(strcmp(azColName[DB_INDEX_DEVELOPER], "developer") == 0);
    assert(strcmp(azColName[DB_INDEX_PUBLISHER], "publisher") == 0);
    assert(strcmp(azColName[DB_INDEX_RELEASE_DATE], "release_date") == 0);
    assert(strcmp(azColName[DB_INDEX_RATING], "rating") == 0);
    assert(strcmp(azColName[DB_INDEX_OVERVIEW], "overview") == 0);


    lv_label_set_text_fmt(synop_text, format,
                DASH_MENU_COLOR, argv[DB_INDEX_TITLE],
                DASH_MENU_COLOR, argv[DB_INDEX_DEVELOPER],
                DASH_MENU_COLOR, argv[DB_INDEX_PUBLISHER],
                DASH_MENU_COLOR, argv[DB_INDEX_RELEASE_DATE],
                DASH_MENU_COLOR, argv[DB_INDEX_RATING],
                DASH_MENU_COLOR, argv[DB_INDEX_OVERVIEW]);

    return 0;
}

static void synop_close(lv_event_t *event)
{
    lv_obj_t *menu = lv_event_get_target(event);
    lv_key_t key = *((lv_key_t *)lv_event_get_param(event));
    if (key == DASH_INFO_PAGE)
    {
        static int key = LV_KEY_ESC;
        lv_event_send(menu, LV_EVENT_KEY, &key);
    }
}

void dash_synop_open(int id)
{
    lv_obj_t *window = container_open();

    // Create a label to store the synopsis information
    lv_obj_t *synop_text = lv_label_create(window);
    lv_label_set_recolor(synop_text, true);
    lv_obj_set_size(synop_text, lv_obj_get_width(window), LV_SIZE_CONTENT);
    lv_label_set_long_mode(synop_text, LV_LABEL_LONG_WRAP);

    // Read synop info from database
    char *cmd = lv_mem_alloc(SQL_MAX_COMMAND_LEN);
    lv_snprintf(cmd, SQL_MAX_COMMAND_LEN, SQL_TITLE_GET_BY_ID, id);
    db_command_with_callback(cmd, synop_info_callback, synop_text);
    lv_mem_free(cmd);
    lv_obj_update_layout(window);
    

    //Register a callback to close the window is we press the DASH_INFO_PAGE key
    lv_obj_add_event_cb(window, synop_close, LV_EVENT_KEY, NULL);
}
