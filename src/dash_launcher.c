// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"


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
            char *folder_path = lv_strdup(file_path);
            char *last = strrchr(folder_path, '\\');
            if (last != NULL)
            {
                last[0] = '\0'; // Knock off file name
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
                const char *file_name = strrchr(file_path, '\\') + 1;
                // Should be myiso.1.iso
                char *trimmed = lv_strdup(file_name);
                // Should be myiso.1
                strrchr(trimmed, '.')[0] = '\0';
                // The .1 is only for split isos, so check if it exists then trim it
                int length = strlen(trimmed);
                if(length > 2 && trimmed[length - 2] == '.' && isdigit(trimmed[length - 1]))
                {
                    trimmed[length - 2] = '\0';
                }

                // Should be "myiso"
                strncpy(title, trimmed, MAX_META_LEN);
                lv_mem_free(trimmed);
            }
            return true;
        }
    }
    return false;
}

bool dash_launcher_is_launchable(const char *file_path)
{
    if (dash_launcher_is_xbe(file_path, NULL, NULL))
    {
        return true;
    }
    #ifdef NXDK
    if (dash_launcher_is_iso(file_path, NULL, NULL))
    {
        uint32_t supported_isos = platform_iso_supported();
        const char *ext = file_path + strlen(file_path) - 4;
        if (strcasecmp(ext, ".cci") == 0 && (supported_isos & PLATFORM_XBOX_CCI_SUPPORTED) == 0)
        {
            //Tried to launch CCI on unsupported BIOS
            lv_label_set_text(lv_label_create(container_open()), "ERROR: Tried to launch CCI on unsupported BIOS");
            return false;
        }
        else if (strcasecmp(ext, ".cso") == 0 && (supported_isos & PLATFORM_XBOX_CSO_SUPPORTED) == 0)
        {
            //Tried to launch CSO on unsupported BIOS
            lv_label_set_text(lv_label_create(container_open()), "ERROR: Tried to launch CSO on unsupported BIOS");
            return false;
        }
        else if (strcasecmp(ext, ".iso") == 0 && (supported_isos & PLATFORM_XBOX_ISO_SUPPORTED) == 0)
        {
            //Tried to launch ISO on unsupported BIOS
            lv_label_set_text(lv_label_create(container_open()), "ERROR: Tried to launch ISO on unsupported BIOS");
            return false;
        }
        return true;
    }
    #endif
    return false;
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
