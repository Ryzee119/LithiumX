// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

#define FOLDER_COLOR "#F2E25D"
#define FILE_COLOR "#FFFFFF"

static void list_dir(const char *path, char **list, int *cnt);
static void dash_browser_draw_hook(lv_event_t *e);

typedef struct 
{
    char *cwd;
    char **list;
    lv_obj_t *menu;
    menu_items_t *items;
    int item_cnt;
    browser_item_selection_cb cb;
} dash_browser_info_t;

static bool is_folder(const char *path)
{
    int len = strlen(path);
    return path[len + 1] == 1;
}

static void browser_item_selected(void *param)
{
    dash_browser_info_t *dinfo = param;
    unsigned short r,c;
    lv_table_get_selected_cell(dinfo->menu, &r, &c);
    char cwd[DASH_MAX_PATH];
    strcpy(cwd, dinfo->cwd);
    //Path should look like "cwd"
    if (strlen(cwd) > 0) strcat(cwd, "\\");
    //Path should look like "cwd\\"
    strcat(cwd, dinfo->list[r]);
    //Path should look like "cwd\\Folder"

    // Call the user callback to check we have found what we are looking for
    if (dinfo->cb(cwd))
    {
        dash_printf(LEVEL_TRACE, "User claimed %s\n", cwd);
        return;
    }

    // Otherwise do nothing unless its a directory, in which case open it
    if (is_folder(dinfo->list[r]))
    {
        dash_printf(LEVEL_TRACE, "Opening %s\n", cwd);
        dash_browser_open(cwd, dinfo->cb);
    }
}

static void dash_browser_closed(lv_event_t *event)
{
    dash_browser_info_t *dinfo = lv_event_get_user_data(event);
    for (int i = 0; i < dinfo->item_cnt; i++)
    {
        lv_mem_free(dinfo->list[i]);
    }
    lv_mem_free(dinfo->list);
    lv_mem_free(dinfo->items);
    lv_mem_free(dinfo->cwd);
    lv_mem_free(dinfo);
}

void dash_browser_open(char *path, browser_item_selection_cb cb)
{
    int cnt;
    dash_browser_info_t *dinfo = lv_mem_alloc(sizeof(dash_browser_info_t));

    // Get file/folder count
    list_dir(path, NULL, &cnt);
    dash_printf(LEVEL_TRACE, "Found %d files/folders in %s\n", cnt, path);

    // Setup directory struct
    int _cnt = LV_MAX(1, cnt);
    dinfo->item_cnt = cnt;
    dinfo->list = lv_mem_alloc(cnt * sizeof(char *));
    dinfo->items = lv_mem_alloc(cnt * sizeof(menu_items_t));
    dinfo->cwd = lv_mem_alloc(strlen(path) + 1);
    dinfo->cb = cb;

    // Now populate list of files/folders
    list_dir(path, dinfo->list, &cnt);
    strcpy(dinfo->cwd, path);

    // Create the basic menu structure
    for (int i = 0; i < cnt; i++)
    {
        dinfo->items[i].str = ""; // Will be set soon
        dinfo->items[i].cb = browser_item_selected;
        dinfo->items[i].callback_param = dinfo;
        dinfo->items[i].confirm_box = NULL;
    }
    if (cnt == 0)
    {
        dinfo->items[0].str = "Empty";
        dinfo->items[0].cb = NULL;
        dinfo->items[0].callback_param = NULL;
        dinfo->items[0].confirm_box = NULL;
    }
    dinfo->menu = menu_open_static(dinfo->items, _cnt);

    int height = lv_obj_get_style_max_height(dinfo->menu, LV_PART_MAIN);
    lv_obj_set_height(dinfo->menu, height);

    // Create a label that shows the current working diretory above the browser
    lv_obj_t *cwd_label = lv_label_create(lv_obj_get_parent(dinfo->menu));
    lv_obj_add_style(cwd_label, &object_style, LV_PART_MAIN);
    lv_obj_set_style_text_align(cwd_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_static(cwd_label, dinfo->cwd);
    lv_label_set_long_mode(cwd_label, LV_LABEL_LONG_CLIP);
    lv_obj_update_layout(cwd_label);
    lv_obj_set_size(cwd_label, lv_obj_get_width(dinfo->menu), lv_obj_get_height(cwd_label));
    lv_obj_align(cwd_label, LV_ALIGN_TOP_MID, 0, DASH_YMARGIN);

    // No we need to replace each line in the menu with the file/folder name plus a prefix
    // The prefix adds a file or folder symbol and a colour
    char file_color[8];
    lv_color_t c = lv_color_make(dash_settings.theme_colour >> 16,
                                 dash_settings.theme_colour >> 8,
                                 dash_settings.theme_colour >> 0);
    lv_snprintf(file_color, sizeof(file_color), "#%02X%02X%02X",
                c.ch.red, c.ch.green, c.ch.blue);

    for (int i = 0; i < cnt; i++)
    {
        bool f = is_folder(dinfo->list[i]);
        lv_table_set_cell_value_fmt(dinfo->menu, i, 0, "%s %s# %s", 
            f ? FOLDER_COLOR : FILE_COLOR,
            f ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE,
            dinfo->list[i]);
    }

    lv_obj_add_event_cb(dinfo->menu, dash_browser_closed, LV_EVENT_DELETE, dinfo);
    lv_obj_add_event_cb(dinfo->menu, dash_browser_draw_hook, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

static void dash_browser_draw_hook(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_ITEMS)
    {
        // Allow having different colours within the cell text
        dsc->label_dsc->flag |= LV_TEXT_FLAG_RECOLOR;
    }
}

static void list_sort(char **arr, int size) {
    int i, j;
    char *temp;

    for (i = 0; i < size - 1; i++) {
        for (j = 0; j < size - i - 1; j++) {
            char *path1 = arr[j];
            char *path2 = arr[j + 1];
            bool folder1 = is_folder(path1);
            bool folder2 = is_folder(path2);
            if (!folder1 && folder2)
            {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
            else if (folder1 == folder2 && strcasecmp(path1, path2) > 0) {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

static void list_dir(const char *path, char **list, int *cnt)
{
    WIN32_FIND_DATA findData;
    HANDLE hFind;
    char searchPath[DASH_MAX_PATH];
    int i = 0;

    *cnt = 0;

#ifdef NXDK
    //Xbox doesn't have a root drive that shows all partitions so we fake it
    if (strcmp(path, DASH_ROOT_PATH) == 0)
    {
        static const char root_drives[][3] = {"C:", "D:", "E:", "F:", "G:", "R:", "S:",
                                              "V:", "W:", "A:", "B:", "P:", "Q:", "X:", "Y:", "Z:", };
        int _cnt = 0;
        for (int i = 0; i < DASH_ARRAY_SIZE(root_drives); i++)
        {
            if(!nxIsDriveMounted(root_drives[i][0]))
            {
                dash_printf(LEVEL_TRACE, "%s not mounted. Skipping\n", root_drives[i]);
                continue;
            }
            if (list != NULL)
            {
                list[_cnt] = (char *)lv_mem_alloc(4);
                strcpy(list[_cnt], root_drives[i]);
                list[_cnt][3] = 1;
            }
            _cnt++;
        }
        *cnt = _cnt;
        return;
    }
#endif

    lv_snprintf(searchPath, MAX_PATH, "%s\\*", path);
    hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    // Iterate through all files/directories
    do
    {
        if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
        {
            // Allocate memory for the string and copy the file/directory name
            if (list != NULL)
            {
                int len = strlen(findData.cFileName);
                list[i] = (char *)lv_mem_alloc(len + 2);
                list[i][len + 1] = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                strcpy(list[i], findData.cFileName);
            }
            i++;
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    // Sort by directory and files then alphabetically
    if (list)
    {
        list_sort(list, i);
    }

    // Close the handle
    FindClose(hFind);

    // Set the count
    *cnt = i;
}
