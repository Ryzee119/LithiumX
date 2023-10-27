// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

// Globals
toml_table_t *dash_search_paths;
dash_settings_t dash_settings;
char dash_launch_path[DASH_MAX_PATH];

static lv_obj_t *focus_stack[32];
static lv_indev_t *indev;
static lv_group_t *input_group;
static int focus_stack_index;
static char *toml_default = ""
                            "# Use keyword Recent to create that page that stores recently launched titles.\n"
                            "# All paths should use a forward slash \"/\" Do not use \"\\\".\n"
                            "# On a syntax error, it will reset back to default\n"
                            "# Page names must be unique\n"
                            "[[pages]]\n"
                            "name = \"Recent\"\n"
                            "\n"
                            "[[pages]]\n"
                            "name = \"Games\"\n"
                            "paths = [\"E:/Games\", \"F:/Games\", \"G:/Games\"]\n"
                            "\n"
                            "[[pages]]\n"
                            "name = \"Applications\"\n"
                            "paths = [\"E:/Applications\", \"F:/Applications\", \"G:/Applications\",\n"
                            "         \"E:/Apps\", \"F:/Apps\", \"G:/Apps\"]\n"
                            "\n"
                            "[[pages]]\n"
                            "name = \"Homebrew\"\n"
                            "paths = [\"E:/Homebrew\", \"F:/Homebrew\", \"G:/Homebrew\"]\n"
                            "\0";

static bool check_path_toml(char *err_msg, int err_msg_len)
{
    const char *search_path = DASH_SEARCH_PATH_CONFIG;
    bool parse_ok = false;
    char errbuf[256];
    toml_array_t *array;

    FILE *fp = fopen(search_path, "r");
    if (fp != NULL)
    {
        dash_search_paths = toml_parse_file(fp, errbuf, sizeof(errbuf));
        fclose(fp);
        if (dash_search_paths != NULL)
        {
            array = toml_array_in(dash_search_paths, "pages");
            if (array)
            {
                parse_ok = true;
            }
            else
            {
                lv_snprintf(err_msg, err_msg_len,
                    "No \"pages\" entry in %s file. Resetting config to default\n", search_path);
            }
        }
        else
        {
            lv_snprintf(err_msg, err_msg_len,
                "Cannot parse %s, Probably a syntax error. Resetting config to default\n", search_path);
        }
    }

    // Could not find or parse a toml path file. Fallback to default and attempt to write it
    if (parse_ok == false)
    {
        dash_search_paths = toml_parse(toml_default, errbuf, sizeof(errbuf));
        assert(dash_search_paths != NULL);
        fp = fopen(search_path, "wb");
        if (fp)
        {
            fwrite(toml_default, strlen(toml_default), 1, fp);
            fclose(fp);
        }
    }
    return true;
}

void dash_focus_set_final(lv_obj_t *focus)
{
    // Make sure the item is in our input group
    lv_group_add_obj(lv_group_get_default(), focus);
    focus_stack[0] = focus;

}

void dash_focus_change_depth(lv_obj_t *new_focus)
{
    // Make sure the item is in our input group
    lv_group_add_obj(lv_group_get_default(), new_focus);

    // Push the currently focused into onto the stack
    focus_stack[++focus_stack_index] = lv_group_get_focused(lv_group_get_default());

    // Change focus to our new item
    dash_focus_change(new_focus);
}

lv_obj_t *dash_focus_pop_depth()
{
    // Get the last item focus item until we get a valid one then jump to it
    assert(focus_stack_index > 0);
    lv_obj_t *pop = focus_stack[focus_stack_index--];
    while (lv_obj_is_valid(pop) == false)
    {
        assert(focus_stack_index >= 0);
        pop = focus_stack[focus_stack_index--];
    }
    dash_focus_change(pop);
    focus_stack_index = LV_MAX(focus_stack_index, 0);
    return pop;
}

void dash_focus_change(lv_obj_t *new_obj)
{
    lv_group_focus_freeze(lv_group_get_default(), false);
    lv_group_focus_obj(new_obj);
    lv_group_focus_freeze(lv_group_get_default(), true);
}

static void create_warning_box(const char *str)
{
    lv_obj_t *window = container_open();
    lv_obj_t *label = lv_label_create(window);
    lv_obj_set_size(label, lv_obj_get_width(window), LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_fmt(label, "Warning: %s\n", str);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
}

static int db_rebuild_thread_f(void *param)
{
    int *complete = param;
    db_rebuild(dash_search_paths);
    *complete = 1;
    lvgl_getlock();
    lv_obj_clean(lv_scr_act());
    dash_create();
    lvgl_removelock();
    return 0;
}

static int db_rebuild_progress_thread_f(void *param)
{
    lv_obj_t *label = param;
    int *complete = label->user_data;
    while (1)
    {
        if (*complete)
        {
            lv_mem_free(complete);
            break;
        }
        extern int db_rebuild_scanned_items;
        lvgl_getlock();
        lv_label_set_text_fmt(label, "Rebuilding Database, please wait... %d", db_rebuild_scanned_items);
        lvgl_removelock();
        SDL_Delay(100);
    }
    return 0;
}

static char err_msg_toml[256], err_msg_db[256];
static bool in_memory_warning;
void dash_init(void)
{
    err_msg_toml[0] = '\0';
    err_msg_db[0] = '\0';

    focus_stack_index = 0;
    lv_memset(focus_stack, 0, sizeof(focus_stack));

    // Set default settings
    lv_memset(&dash_settings, 0, sizeof(dash_settings));
    dash_settings.magic = DASH_SETTINGS_MAGIC;
    dash_settings.max_recent_items = 15;
    dash_settings.theme_colour = (22 << 16) | (111 << 8) | (15 << 0);

    // Read in the toml file that has all the search paths
    check_path_toml(err_msg_toml, sizeof(err_msg_toml));

    // Setup input devices and a default input group
    input_group = lv_group_create();
    lv_group_set_default(input_group);
    indev = NULL;
    for (;;)
    {
        indev = lv_indev_get_next(indev);
        if (!indev)
        {
            break;
        }
        lv_indev_set_group(indev, input_group);
    }

    // Open up the database. If the database doesnt exist it was created
    // It it couldnt be created on disk, it is created in RAM which is not persistent so
    // will cause a warning.
    in_memory_warning = !db_open();

    // Check that the database is valid (Correct tables, and columns). Otherwise begin a database rebuild
    if (db_init(err_msg_db, sizeof(err_msg_db)) == true)
    {
        dash_create();
    }
    else
    {
        lv_obj_t *window = lv_obj_create(lv_scr_act());
        lv_obj_set_size(window, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
        lv_obj_set_style_bg_color(window, lv_color_make(0,0,0), LV_PART_MAIN);
        lv_obj_t *label = lv_label_create(window);
        lv_obj_center(label);
        lv_label_set_text_static(label, "Rebuilding Database, please wait...");
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        int *complete = lv_mem_alloc(sizeof (int));
        *complete = 0;
        label->user_data = complete;
        SDL_CreateThread(db_rebuild_progress_thread_f, "db_rebuild_progress_thread_f", label);
        SDL_CreateThread(db_rebuild_thread_f, "db_rebuild_thread_f", complete);
    }
    return;
}

static int update_mem_usage_thread(void *param)
{
    lv_obj_t *mem_usage_label = param;
    uint32_t used, capacity;
    while (1)
    {
        lx_mem_usage(&used, &capacity);
        lvgl_getlock();
        lv_label_set_text_fmt(mem_usage_label, "%d / %d", used, capacity);
        lvgl_removelock();
        SDL_Delay(LV_DISP_DEF_REFR_PERIOD);
    }
    return 0;
}


void dash_create()
{
    dash_settings_read();

    // Work out the main theme color from the settings
    lv_color_t col = lv_color_make(dash_settings.theme_colour >> 16,
                                   dash_settings.theme_colour >> 8,
                                   dash_settings.theme_colour >> 0);
    dash_styles_init(col);

    // Create a gradient background
    lv_obj_t *grad = lv_obj_create(lv_scr_act());
    lv_obj_add_style(grad, &dash_background_style, LV_PART_MAIN);
    lv_obj_set_size(grad, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    lv_obj_center(grad);

    // Create the scrollers for gameart
    dash_scroller_init();

    // dash_scroller_init has threads. Use lvgl locks now
    lvgl_getlock();
    dash_scroller_scan_db();
    dash_scroller_set_page();

    if (in_memory_warning)
    {
        create_warning_box("Warning: Could not open database at " DASH_DATABASE_PATH
                                     " Using in memory database only.");
    }
    if (strlen(err_msg_toml) > 0)
    {
        create_warning_box(err_msg_toml);
    }
    if (strlen(err_msg_db) > 0)
    {
        create_warning_box(err_msg_db);
    }

    lv_obj_t *mem_usage_label = lv_label_create(lv_layer_sys());
    lv_obj_set_style_bg_opa(mem_usage_label, LV_OPA_50, 0);
    lv_obj_set_align(mem_usage_label, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_size(mem_usage_label, 100, 50);
    lv_obj_set_style_text_font(mem_usage_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_label_set_text(mem_usage_label, "hello");
    lvgl_removelock();
    SDL_CreateThread(update_mem_usage_thread, "update_mem_usage", mem_usage_label);
}
