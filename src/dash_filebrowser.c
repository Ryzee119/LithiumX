// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"
#include "helpers/menu.h"
#include "helpers/fileio.h"
#include "helpers/nano_debug.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef NXDK
#include <nxdk/mount.h>
static const char root_drives[][3] = {"C:", "D:", "E:", "F:", "G:", "X:", "Y:", "Z:"};
#define ROOT_PATH "A:"
#else
#define ROOT_PATH "A:."
#endif
static uint8_t row_stack[256];
static uint8_t row_stack_i;
static lv_obj_t *file_browser_container;
static lv_obj_t *file_browser;
static lv_obj_t *cwd_label;

#define FOLDER_COLOR "#F2E25D"
#define FILE_COLOR "#FFFFFF"

static int file_sort(const void *a, const void *b)
{
    char *const *pp1 = a;
    char *const *pp2 = b;
    char *str1 = *pp1;
    char *str2 = *pp2;
    return strcasecmp(str1, str2);
}

static bool list_dir(lv_obj_t *obj)
{
    lv_fs_dir_t dir;
    bool ret = false;
    char fname[DASH_MAX_PATHLEN];
    char *cwd = (char *)obj->user_data;
    int row = 0;

    if (strcmp(ROOT_PATH, cwd) == 0)
    {
#ifdef NXDK
        int drives = sizeof(root_drives) / sizeof(root_drives[0]);
        lv_label_set_text(cwd_label, &cwd[2]);
        lv_table_set_row_cnt(obj, drives);
        for (int i = 0; i < drives; i++)
        {
            if (nxIsDriveMounted(root_drives[i][0]))
            {
                lv_table_set_cell_value_fmt(obj, row++, 0, "%s %s# %s",
                                            FOLDER_COLOR, LV_SYMBOL_DIRECTORY, root_drives[i]);
            }
        }
        lv_table_set_row_cnt(obj, row);
        return true;
#endif
    }

    if (lv_fs_dir_open(&dir, cwd) == LV_FS_RES_OK || lv_fs_exists(cwd))
    {
        ret = true;
        lv_label_set_text(cwd_label, &cwd[2]);
        // Minimise reallocs in lvgl by preallocating a bunch of rows
        lv_table_set_row_cnt(obj, 1024);
        while (dir.dir_d && lv_fs_dir_read(&dir, fname) == LV_FS_RES_OK)
        {
            if (strlen(fname) == 0)
            {
                break;
            }
            lv_table_add_cell_ctrl(obj, row, 0, LV_TABLE_CELL_CTRL_TEXT_CROP);
            lv_table_set_cell_value_fmt(obj, row++, 0, "%s %s# %s",
                                        (fname[0] == '/') ? FOLDER_COLOR : FILE_COLOR,
                                        (fname[0] == '/') ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE,
                                        (fname[0] == '/') ? &fname[1] : &fname[0]);
        }
        if (ret)
        {
            lv_table_set_row_cnt(obj, row);
            lv_fs_dir_close(&dir);
        }
        lv_table_t *t = (lv_table_t *)obj;
        qsort(t->cell_data, t->row_cnt, sizeof(char *), file_sort);
    }
    return ret;
}

static void async_invalidate(void *obj)
{
    lv_obj_invalidate(obj);
}

// Callback when a key/button is pressed when the file browser is in focus
static void table_key(lv_event_t *e)
{
    uint16_t row, col;
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    lv_obj_t *obj = lv_event_get_target(e);
    lv_table_t *t = (lv_table_t *)obj;
    char old_cwd[DASH_MAX_PATHLEN];
    char *cwd = (char *)obj->user_data;
    strcpy(old_cwd, cwd);

    lv_table_get_selected_cell(obj, &row, &col);

    if (row == LV_TABLE_CELL_NONE)
    {
        return;
    }
    if (key == DASH_INFO_PAGE)
    {
        menu_hide_item(file_browser_container);
    }
    else if (key == LV_KEY_ESC)
    {
        if (strcmp(cwd, ROOT_PATH) == 0)
        {
            menu_hide_item(file_browser_container);
            return;
        }
#ifdef NXDK
        // Path fix for root dir for xbox
        if (strlen(cwd) == 4 && cwd[2] != '.')
        {
            cwd[2] = '\0';
        }
#endif
        lv_fs_up(cwd);
        // List the previous directory
        if (!list_dir(obj))
        {
            // If it fails, restore the old cwd
            strcpy(cwd, old_cwd);
        }
        else
        {
            // Restore the old row that was selected, and select it.
            t->row_act = row_stack[--row_stack_i];
        }
    }
    else if (key == LV_KEY_ENTER)
    {
        const char *cell_str = lv_table_get_cell_value(obj, row, col);
        // table cell text is like "#COLOR#SYMBOL "file/folder name",
        // we need to cut off #COLOR #SYMBOL chars to get the fname.
        const char *fname = &cell_str[sizeof(FOLDER_COLOR) +
                                      sizeof(LV_SYMBOL_DIRECTORY) + 1];
        if (strstr(cell_str, LV_SYMBOL_DIRECTORY))
        {
            // Should now have "A:cwd"
            int end = strlen(cwd);
#ifdef NXDK
            if (strcmp(ROOT_PATH, cwd) != 0)
#endif
            {
                cwd[end++] = DASH_PATH_SEPARATOR;
            }
            // Should now have "A:cwd/"
            strcpy(&cwd[end], fname);
            // Should now have "A:cwd/newfolder"

            // List the new directory.
            if (!list_dir(obj))
            {
                // If it fails. Restore the original cwd.
                strcpy(cwd, old_cwd);
            }
            else
            {
                // Push the old row onto the stack to remember it
                row_stack[row_stack_i++] = row;
                // Reset the selected row to 0 for the new folder
                t->row_act = 0;
            }
        }
        else
        {
            // Its a file. Check if its extension is the same as DASH_LAUNCH_EXE,
            // then open a confirm box for launch
            const char *ext = lv_fs_get_ext(fname);
            const char *launch_ext = lv_fs_get_ext(DASH_LAUNCH_EXE);
            if (strcasecmp(ext, launch_ext) == 0)
            {
                dash_set_launch_exe("%s%c%s", cwd, DASH_PATH_SEPARATOR, fname);
                confirmbox_open(dash_launch_title, "%s \"%s%c%s\"",
                                "Launch", &cwd[2], DASH_PATH_SEPARATOR, fname);
            }
        }
    }
    // Faster scroll with LB and RB
    else if (key == 'L' || key == 'R')
    {
        int new_row = (int)t->row_act;
        new_row += (key == 'L') ? -8 : 8;
        new_row = LV_MAX(0, new_row);
        new_row = LV_MIN(t->row_cnt - 1, new_row);
        t->row_act = new_row;
    }
    menu_table_scroll(e);
    lv_async_call(async_invalidate, obj);
}

// Called when the file list table is about to be drawn. We can tweak draw parameters here
static void table_draw_tweak(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_ITEMS)
    {
        // Allow having different colours within the cell text
        dsc->label_dsc->flag |= LV_TEXT_FLAG_RECOLOR;
    }
}

static void table_refocus(lv_event_t *e)
{
    // When the main container is visible we want to automatically select the file browser table
    lv_group_t *gp = lv_group_get_default();
    lv_group_focus_freeze(gp, false);
    lv_group_focus_obj(file_browser);
    lv_group_focus_freeze(gp, true);
}

void file_browser_init(void)
{
    // The filebrowser is made up of a generic container. This container has
    // a header label that displayes the current working directory (cwd) and a table
    // that lists of the files and folders.
    file_browser_container = lv_obj_create(lv_scr_act());
    lv_group_add_obj(lv_group_get_default(), file_browser_container);
    menu_apply_style(file_browser_container);
    lv_obj_set_style_pad_row(file_browser_container, 0, LV_PART_MAIN);
    lv_obj_set_size(file_browser_container, MENU_WIDTH, MENU_HEIGHT);
    file_browser_container->user_data = lv_mem_alloc(sizeof(menu_data_t));
    lv_obj_set_flex_flow(file_browser_container, LV_FLEX_FLOW_COLUMN);

    cwd_label = lv_label_create(file_browser_container);
    menu_apply_style(cwd_label);
    lv_obj_set_style_border_width(cwd_label, 0, LV_PART_MAIN);
    lv_obj_set_size(cwd_label, MENU_WIDTH, LV_SIZE_CONTENT);
    lv_obj_update_layout(cwd_label);
    lv_label_set_long_mode(cwd_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(cwd_label, "");

    file_browser = lv_table_create(file_browser_container);
    menu_apply_style(file_browser);
    lv_obj_set_size(file_browser, MENU_WIDTH, MENU_HEIGHT - lv_obj_get_height(cwd_label));
    lv_table_set_col_width(file_browser, 0, MENU_WIDTH);

    file_browser->user_data = lv_mem_alloc(DASH_MAX_PATHLEN);
    strcpy((char *)file_browser->user_data, ROOT_PATH);

    lv_obj_add_event_cb(file_browser, menu_table_scroll, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(file_browser, table_key, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(file_browser, table_draw_tweak, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(file_browser_container, table_refocus, LV_EVENT_FOCUSED, NULL);

    // Maintain a push/pop stack to remember the selected folder for parent directories
    lv_memset(row_stack, 0, sizeof(row_stack));
    row_stack_i = 0;

    // Initally populate the directory listing
    list_dir(file_browser);

    // Hidden for now
    lv_obj_add_flag(file_browser_container, LV_OBJ_FLAG_HIDDEN);
}

void file_browser_deinit(void)
{
    lv_mem_free(file_browser->user_data);
    lv_obj_del(file_browser_container);
    file_browser_container = NULL;
}

void file_browser_open(void)
{
    nano_debug(LEVEL_TRACE, "TRACE: Opening file browser\n");
    menu_show_item(file_browser_container, NULL);
}
