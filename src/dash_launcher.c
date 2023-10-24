// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

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


bool dash_launcher_is_xbe(const char *file_path, char *title, char *title_id)
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

bool dash_launcher_is_iso(const char *file_path, char *title, char *title_id)
{
    const int extlen = 4; //".iso" | ".cso" | ".cci"
    int slen = strlen(file_path);
    if (slen > extlen)
    {
        const char *ext = file_path + slen - extlen;
        if (strcasecmp(ext, ".iso") == 0 || strcasecmp(ext, ".cso") == 0 || strcasecmp(ext, ".cci") == 0)
        {
            if (title)
            {
                // Need to convert /mypath/myiso.1.iso (for example) to "myiso"
                char *folder_path = lv_mem_alloc(strlen(file_path) + 1);
                strcpy(folder_path, file_path);

                const char *cfile_name = last_backslash(folder_path) + 1;
                char *file_name = lv_mem_alloc(strlen(cfile_name) + 1);
                strcpy(file_name, cfile_name);

                file_name[strlen(file_name) - extlen] = '\0';
                int file_name_length = strlen(file_name);

                if (file_name_length > 2 && file_name[file_name_length - 2] == '.')
                {
                    file_name[file_name_length - 2] = '\0';
                }
                strncpy(title, file_name, MAX_META_LEN);
                DbgPrint("%s\n", title);
                lv_mem_free(folder_path);
            }
            return true;
        }
    }
    return false;
}

bool dash_launcher_is_launchable(const char *file_path)
{
    return dash_launcher_is_xbe(file_path, NULL, NULL) || dash_launcher_is_iso(file_path, NULL, NULL);
}

void dash_launcher_go(const char *selected_path)
{
    static const char *no_meta = "No Meta-Data";
    char cmd[SQL_MAX_COMMAND_LEN];
    char time_str[20];
    launch_param_t *launch_params = lv_mem_alloc(sizeof(launch_param_t));

    platform_get_iso8601_time(time_str);

    strcpy(launch_params->selected_path, selected_path);

    if (dash_launcher_is_xbe(selected_path, launch_params->title, launch_params->title_id) == false)
    {
        if (dash_launcher_is_iso(selected_path, launch_params->title, launch_params->title_id) == false)
        {
            lv_mem_free(launch_params);
            return;
        }
    }

    DbgPrint("dash launch path %s selected_path \"%s\" \n", launch_params->selected_path, selected_path);

    // See if the launch paths exists in page "Recent"
    const char *query = "SELECT " SQL_TITLE_DB_ID " FROM " SQL_TITLES_NAME
                        " WHERE " SQL_TITLE_LAUNCH_PATH "= \"%s\" AND " SQL_TITLE_PAGE " = \"__RECENT__\"";
    lv_snprintf(cmd, sizeof(cmd), query, launch_params->selected_path);
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
            launch_params->title_id,
            launch_params->title,
            launch_params->selected_path,
            "__RECENT__",
            no_meta,
            no_meta,
            no_meta,
            no_meta,
            time_str,
            "0.0");
    }

    // Setup launch path then quit
    strcpy(dash_launch_path, launch_params->selected_path);
    lv_set_quit(LV_QUIT_OTHER);

    lv_mem_free(launch_params);
    return;
}
