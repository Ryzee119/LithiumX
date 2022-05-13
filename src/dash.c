// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"
#include "dash_titlelist.h"
#include "dash_synop.h"
#include "dash_menu.h"
#include "helpers/fileio.h"
#include "jpg_decoder.h"
#include "helpers/nano_debug.h"
#include "xml/src/xml.h"
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>

#ifdef NXDK
#ifdef DBG
#include <xboxkrnl/xboxkrnl.h>
static void mem_callback(lv_timer_t *event)
{
    lv_obj_t *label = event->user_data;
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    uint32_t mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE) / 1024U;
    uint32_t mem_used = mem_size - ((MemoryStatistics.AvailablePages * PAGE_SIZE) / 1024U);
    lv_label_set_text_fmt(label, "%d/%dkb", mem_used, mem_size);
}
#endif
#endif

#define ROOT_PARENT lv_scr_act()

/* clang-format off */
static const char *xml_default = ""
"<Root>\n"
  "\t<dash_pages>\n"
      "\t\t<Recent>\n"
            "\t\t\t<1>Recent</1>\n"
      "\t\t</Recent>\n"
      "\t\t<Games>\n"
            "\t\t\t<1>A:\\Games</1>\n"
            "\t\t\t<2>E:\\Games</2>\n"
            "\t\t\t<3>F:\\Games</3>\n"
            "\t\t\t<4>G:\\Games</4>\n"
      "\t\t</Games>\n"
      "\t\t<Applications>\n"
            "\t\t\t<1>E:\\Apps</1>\n"
            "\t\t\t<2>F:\\Apps</2>\n"
            "\t\t\t<3>G:\\Apps</3>\n"
            "\t\t\t<4>E:\\Applications</4>\n"
            "\t\t\t<5>F:\\Applications</5>\n"
            "\t\t\t<6>G:\\Applications</6>\n"
      "\t\t</Applications>\n"
      "\t\t<Homebrew>\n"
            "\t\t\t<1>E:\\Homebrew</1>\n"
            "\t\t\t<2>F:\\Homebrew</2>\n"
            "\t\t\t<3>G:\\Homebrew</3>\n"
      "\t\t</Homebrew>\n"
  "\t</dash_pages>\n"
  "\t<dash_settings>\n"
      "\t\t<use_fahrenheit>0</use_fahrenheit>\n"
      "\t\t<default_screen_index>0</default_screen_index>\n"
      "\t\t<auto_launch_dvd>0</auto_launch_dvd>\n"
  "\t</dash_settings>\n"
"</Root>\0";
/* clang-format on */

// There is one 'parser' per 'tile'. The parser asynchronously parses all the path set my the xml and adds
// eatch item. Each parser contains a image scrolling container 'scroller' to show all the game art etc.
// Each 'tile' is a child of a tileview object 'pagetiles'. These are swiped left and right to change page.
typedef struct
{
    lv_obj_t *tile;                       // What tile in the tileview parent 'pagetiles'
    lv_obj_t *scroller;                   // The scriller contains image containers for each item
    lv_fs_dir_t dir_handle;               // Track the current directory handle for parsing
    int num_paths;                        // How many search paths were found in the xml
    int current_path_index;               // Track the current path being seached up to num_paths
    int current_item;                     // Track the currently focussed item in this parser
    char *paths[DASH_MAX_PATHS_PER_PAGE]; // Store all the search paths found in the XML
    char cwd[DASH_MAX_PATHLEN];           // Track the current working directory (for scanning subfolders etc)
    title_t title[DASH_MAX_GAMES];        // A memory pool for all the titles found in this parser
} parse_handle_t;

static bool dash_running = false;
static uint8_t *xml_raw;
static lv_obj_t *page_tiles;
static lv_obj_t *label_footer;

static int parser_cnt;
static SDL_Thread *parser_thread;
static parse_handle_t parsers[DASH_MAX_PAGES];
static parse_handle_t *parser_current;

struct xml_document *dash_config;
struct xml_node *dash_config_root, *dash_pages, *dash_settings;

static parse_handle_t *recent_parser;
static char **recent_titles;

static unsigned int current_tile_index;
unsigned int settings_use_fahrenheit = 0;
unsigned int settings_default_screen_index = 0;
unsigned int settings_auto_launch_dvd = 0;

// Helper function to change page. Input is the new page index
// This has bounds checking and will update the parsing priority
static void change_page(int new_index)
{
    // Bounds check
    new_index = LV_MAX(0, new_index);
    new_index = LV_MIN(parser_cnt - 1, new_index);

    // Apply it
    current_tile_index = new_index;
    parser_current = &parsers[current_tile_index];
    if (lv_obj_get_child_cnt(parser_current->scroller) == 0)
    {
        lv_group_focus_obj(parser_current->scroller);
        lv_label_set_text(label_footer, "No items found");
    }
    else if(lv_obj_is_valid(lv_obj_get_child(parser_current->scroller, parser_current->current_item)))
    {
        lv_group_focus_obj(lv_obj_get_child(parser_current->scroller, parser_current->current_item));
    }
    else
    {
        lv_group_focus_obj(parser_current->scroller);
    }
    lv_obj_set_tile_id(page_tiles, current_tile_index, 0, LV_ANIM_ON);
}

// About to launch a title. Add it to the recent titles file (if enabled) and launch it
static void launch_title(void)
{
    lv_fs_file_t fp;
    uint32_t brw;
    const char *launch_folder = dash_get_launch_folder();

    // Recent items page isnt enabled, so just prepare launch and leave
    if (recent_parser == NULL)
    {
        goto leave;
    }

    // We're about to launch an item. We add it to the recent items file.
    // If it's a duplicate we knock it out of the list so it gets added back up front
    int num_items = lv_obj_get_child_cnt(recent_parser->scroller);
    for (int i = 0; i < num_items; i++)
    {
        if (strcmp(recent_titles[i], launch_folder) == 0)
        {
            for (int k = i; k < num_items; k++)
            {
                recent_titles[k] = recent_titles[k + 1];
            }
            num_items--;
            break;
        }
    }
    // Create a new list to hold to reordered items. We cap it to a max for some sanity
    // Most recent item is always first on the list. If its already on the list, it gets pushed to first.
    // If its not on the list it is inserted. If the number if items exceed RECENT_TITLES_MAX, the last item is dropped.
    int recent_title_cnt_new = LV_MIN(RECENT_TITLES_MAX, num_items + 1);
    const char **recent_titles_new = (const char **)lv_mem_alloc(sizeof(char *) * recent_title_cnt_new);
    recent_titles_new[0] = launch_folder;
    for (int i = 1; i < recent_title_cnt_new; i++)
    {
        recent_titles_new[i] = recent_titles[i - 1];
    }
    if (lv_fs_open(&fp, RECENT_TITLES, LV_FS_MODE_WR) == LV_FS_RES_OK)
    {
        for (int i = 0; i < recent_title_cnt_new; i++)
        {
            lv_fs_write(&fp, recent_titles_new[i], strlen(recent_titles_new[i]), &brw);
            lv_fs_write(&fp, "\n", 1, &brw); // Needs to be finished with a new line
        }
        lv_fs_close(&fp);
    }
    lv_mem_free(recent_titles_new);
leave:
    lv_set_quit(LV_QUIT_OTHER);
}

// User input callback function.
// This handles changing page (LB, RB), selecting a new titles (Up,Down,Left,Right)
// show the game info screen (synopsis screen) and the settings screen (Main menu)
static void input_callback(lv_event_t *event)
{
    title_t *current_title = NULL, *new_title = NULL;
    lv_obj_t *current_obj, *new_obj, *parent;
    int current_index, new_index, last_index;

    // Work out what caused the event (Key press or focus change etc)
    lv_event_code_t e = lv_event_get_code(event);

    // Remember the active screen we are on
    parse_handle_t *p = (parse_handle_t *)event->user_data;

    // Store the obj that was active when the event occured
    current_obj = lv_event_get_target(event);

    // The obj has the title_t struct stored with it
    current_title = (title_t *)current_obj->user_data;

    if (e == LV_EVENT_KEY)
    {
        parent = p->scroller;
        last_index = lv_obj_get_child_cnt(parent) - 1; // The index of the final item in the scroller
        current_index = lv_obj_get_index(current_obj); // The index of the current item in the scroller
        new_index = current_index;                     // Will store the index we need to go to if it changes
        lv_key_t key = lv_indev_get_key(lv_indev_get_act());

        // Change page
        if (key == DASH_PREV_PAGE || key == DASH_NEXT_PAGE)
        {
            current_tile_index += (key == DASH_NEXT_PAGE) ? (1) : -1;
            change_page(current_tile_index);
            return;
        }

        // Show settings page (Main menu)
        if (key == DASH_SETTINGS_PAGE)
        {
            main_menu_open();
            return;
        }

        // If this page has no items, we are done
        if (lv_obj_get_child_cnt(parser_current->scroller) == 0)
        {
            return;
        }

        // Show synopsis page
        if (key == DASH_INFO_PAGE)
        {
            synop_menu_open(current_title);
            return;
        }

        // Launch Title
        if (key == LV_KEY_ENTER)
        {
            dash_set_launch_folder(current_title->title_folder);
            char *confirm_box_text = (char *)lv_mem_alloc(DASH_MAX_PATHLEN);
            lv_snprintf(confirm_box_text, DASH_MAX_PATHLEN, "%s \"%s\"", "Launch", current_title->title);
            menu_create_confirm_box(confirm_box_text, launch_title);
            lv_mem_free(confirm_box_text);
            return;
        }

        // Move focus onto next item. (L and R are the back triggers)
        if (key == LV_KEY_RIGHT || key == LV_KEY_LEFT || key == LV_KEY_UP || key == LV_KEY_DOWN || key == 'L' || key == 'R')
        {
            // FIXME: Should really allow thumbnails of any width
            int tiles_per_row = lv_obj_get_width(parent) / DASH_THUMBNAIL_WIDTH;

            // At the start, loop to end
            if (current_index == 0 && (key == LV_KEY_LEFT || key == LV_KEY_UP))
            {
                new_index = last_index;
            }
            // At the end, loop to start
            else if (current_index == last_index && (key == LV_KEY_RIGHT || key == LV_KEY_DOWN))
            {
                new_index = 0;
            }
            // Increment left or right one
            else if (key == LV_KEY_RIGHT || key == LV_KEY_LEFT)
            {
                new_index += (key == LV_KEY_RIGHT) ? 1 : -1;
            }
            // Increment up or down one
            else if (key == LV_KEY_UP || key == LV_KEY_DOWN)
            {
                new_index += (key == LV_KEY_DOWN) ? tiles_per_row : -tiles_per_row;
            }
            // Increment up or down lots (LT and RT)
            else if (key == 'L' || key == 'R')
            {
                new_index += (key == 'R') ? (tiles_per_row * 8) : -(tiles_per_row * 8);
            }
            // Bounds check it
            new_index = LV_MAX(0, new_index);
            new_index = LV_MIN(last_index, new_index);
        }

        // Grab the object at the new index
        new_obj = lv_obj_get_child(parent, new_index);
        LV_ASSERT(new_obj != NULL); // Some unhandled case! Shouldnt happen
        new_title = (title_t *)new_obj->user_data;
        LV_ASSERT(new_title != NULL);

        // Remember the current index so when we change page we come back to the previous item
        p->current_item = new_index;

        // Scroll to the new item if its off screen and focus it
        lv_obj_scroll_to_view_recursive(new_title->image_container, LV_ANIM_ON);
        lv_group_focus_obj(new_obj);
        return;
    }
    // Add a border around focused game and update footer text
    else if (e == LV_EVENT_FOCUSED)
    {
        lv_obj_set_style_border_width(current_obj, 5, LV_PART_MAIN);
        lv_label_set_text(label_footer, current_title->title);
    }
    // Clear border around previously focused game
    else if (e == LV_EVENT_DEFOCUSED)
    {
        lv_obj_set_style_border_width(current_obj, 0, LV_PART_MAIN);
    }
}

// Function that cycles through all directors and subdirectories setup in the xml and looks for
// files with the name set by DASH_LAUNCH_EXE. If found, these are added to the respective scroller.
// This is called via game_parser_thread
static void game_parser_task(parse_handle_t *p)
{
    char fname[DASH_MAX_PATHLEN];
    int len;
    lv_fs_dir_t sub_dir;
    int remaining = p->num_paths - p->current_path_index;
    if (remaining <= 0)
    {
        return;
    }
    // Opening the next main directory for parsing
    if (p->dir_handle.drv == NULL)
    {
        // lvgl expects LV_FS_STDIO_LETTER at the beginning of each file path to ensure we use the correct fs_driver.
        lv_snprintf(p->cwd, sizeof(p->cwd), "%c:%s", LV_FS_STDIO_LETTER, p->paths[p->current_path_index]);
        if (lv_fs_dir_open(&p->dir_handle, p->cwd) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_WARN, "WARN: Could not open %s. Skipping\n", p->cwd);
            goto done_path;
        }
    }
    // Read the next file/subdirectory within the main directory. If fail or strlen == 0, no more files
    if (lv_fs_dir_read(&p->dir_handle, fname) != LV_FS_RES_OK)
    {
        goto done_path;
    }
    if (strlen(fname) == 0)
    {
        goto done_path;
    }
    // We found a folder (Folders begin with '/' in lvgl)
    if (fname[0] == '/')
    {
        // cwd should look like A:PARSE_DIR
        len = strlen(p->cwd);
        char *end = &p->cwd[len];
        *end = DASH_PATH_SEPARATOR;
        end++;
        strcpy(end, &fname[1]); // Skip the first '/' symbol
        // cwd should look like A:PARSE_DIR\SUB_DIR
        if (lv_fs_dir_open(&sub_dir, p->cwd) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_WARN, "WARN: Could not open sub path %s. Skipping\n", p->cwd);
            goto done_subfolder;
        }
        // Scan all files within subfolder for the launch filename
        while (lv_fs_dir_read(&sub_dir, fname) == LV_FS_RES_OK)
        {
            // No more files
            if (fname[0] == '\0')
            {
                break;
            }
            if (strncmp(fname, DASH_LAUNCH_EXE, sizeof(fname)) != 0)
            {
                continue;
            }
            if (lv_obj_get_child_cnt(p->scroller) == DASH_MAX_GAMES)
            {
                nano_debug(LEVEL_WARN, "WARN: Reached max title limit of %d, increase DASH_MAX_GAMES\n", DASH_MAX_GAMES);
                break;
            }

            // Looks like we found a launch filename. Add it to the list and register the appropriate callbacks
            title_t *new_title = &p->title[lv_obj_get_child_cnt(p->scroller)];
            if (titlelist_add(new_title, p->cwd, p->scroller) != 0)
            {
                nano_debug(LEVEL_TRACE, "TRACE: Found item %s\n", new_title->title);
                lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_KEY, p);
                lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_FOCUSED, p);
                lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_DEFOCUSED, p);
            }
        }
        goto done_subfolder;
    }
done_subfolder:
    lv_fs_dir_close(&sub_dir);
    lv_fs_up(p->cwd);
    return;
done_path:
    lv_fs_dir_close(&p->dir_handle);
    p->current_path_index++;
    return;
}

// qsort function to sort titles alphabetically as they are added
static int title_sort(const void *a, const void *b)
{
    lv_obj_t **_a = (lv_obj_t **)a;
    lv_obj_t **_b = (lv_obj_t **)b;
    title_t *_title_a = (title_t *)_a[0]->user_data;
    title_t *_title_b = (title_t *)_b[0]->user_data;
    // FIXME. use strcasecmp when available in nxdk
    if (_title_a->title[0] >= 'a' && _title_a->title[0] <= 'z')
    {
        _title_a->title[0] -= 'a' - 'A';
    }
    if (_title_b->title[0] >= 'a' && _title_b->title[0] <= 'z')
    {
        _title_b->title[0] -= 'a' - 'A';
    }
    return strcmp(_title_a->title, _title_b->title);
}

// Thread to parse all titles. This is in a separate thread to keep GUI responsive while it is parsing and to
// reduce load times. This uses locks to interface with lvgl which is not normally thread-safe.
static int game_parser_thread(void *ptr)
{
    size_t remaining;
    while (1)
    {
        remaining = 0;
        if (dash_running == false)
        {
            return 0;
        }
        for (int i = 0; i < parser_cnt; i++)
        {
            lvgl_getlock();
            game_parser_task(&parsers[i]);
            remaining += parsers[i].num_paths - parsers[i].current_path_index;
            lvgl_removelock();

            for (int k = 0; k < parser_cnt; k++)
            {
                lvgl_getlock();
                // As items are being added, they may not be in alphabetic order
                // We sort them occassionally and once when all items have been parsed.
                // We also dont sort the recent titles page
                uint32_t child_cnt = lv_obj_get_child_cnt(parsers[k].scroller);
                if ((remaining == 0 || (child_cnt % 32) == 0) && child_cnt > 1 && &parsers[k] != recent_parser)
                {
                    qsort(parsers[k].scroller->spec_attr->children,
                          lv_obj_get_child_cnt(parsers[k].scroller), sizeof(lv_obj_t *), title_sort);
                }
                lvgl_removelock();
            }
        }
        // All times in all folders have been parsed. We can see all the path strings and close the thread.
        if (remaining == 0)
        {
            lvgl_getlock();
            for (int k = 0; k < parser_cnt; k++)
            {
                parse_handle_t *p = &parsers[k];
                for (int j = 0; j < p->num_paths; j++)
                {
                    lv_mem_free(p->paths[j]);
                    p->paths[j] = NULL;
                }
            }
            lvgl_removelock();
            // We are done with the thread. Leave
            return 0;
        }
        // This thread is slow priority. Yield to other threads for a bit
        SDL_Delay(0);
    }
    return 0;
}

void dash_init(void)
{
    if (dash_running)
    {
        return;
    }
    dash_running = true;
    lv_fs_file_t fp;
    uint32_t fs;
    bool xml_parse_error = true;
    unsigned int br, bw;
    const char *xml_settings_path = DASH_XML;

    // Read the xml path if available to gather settings and search paths
    xml_raw = lv_fs_orc(xml_settings_path, &br);
    if (xml_raw != NULL)
    {
        dash_config = xml_parse_document((uint8_t *)xml_raw, strlen((char *)xml_raw));
        if (dash_config != 0)
        {
            // Get the root of the xml file. The xml should have two child nodes. "dash_pages" and "dash_settings"
            dash_config_root = xml_document_root(dash_config);
            // Get the child notes
            dash_pages = xml_easy_child(dash_config_root, (const uint8_t *)"dash_pages", 0);
            dash_settings = xml_easy_child(dash_config_root, (const uint8_t *)"dash_settings", 0);
            // Nodes under "dash_pages" detemine how many pages we need
            uint32_t pages = (dash_pages) ? xml_node_children(dash_pages) : 0;
            // Check if everything looks valid
            if (dash_pages && dash_settings && pages > 0)
            {
                nano_debug(LEVEL_TRACE, "TRACE: Found %s\n", xml_settings_path);
                xml_parse_error = false;
            }
            else
            {
                nano_debug(LEVEL_ERROR, "ERROR: Invalid xml at %s\n", xml_settings_path);
                xml_document_free(dash_config, false);
                lv_mem_free(xml_raw);
            }
        }
    }
    // If xml file wasnt found, or it was invalid or some other error occur we revert to a hardcoded
    // internal xml file. This new file will be setup and written to the launching directory
    if (xml_parse_error)
    {
        // We use built xml in. Assert on errors here, as errors should never happen!
        nano_debug(LEVEL_WARN, "WARN: %s missing or invalid. Using inbuilt default", DASH_XML);
        int xml_len = strlen(xml_default);
        xml_raw = (uint8_t *)lv_mem_alloc(xml_len + 1);
        lv_memcpy(xml_raw, xml_default, xml_len);
        xml_raw[xml_len] = '\0';
        dash_config = xml_parse_document((uint8_t *)xml_raw, strlen((char *)xml_raw));
        LV_ASSERT(dash_config != 0);
        dash_config_root = xml_document_root(dash_config);
        LV_ASSERT(dash_config_root != 0);
        dash_pages = xml_easy_child(dash_config_root, (const uint8_t *)"dash_pages", 0);
        LV_ASSERT(dash_pages != 0);
        dash_settings = xml_easy_child(dash_config_root, (const uint8_t *)"dash_settings", 0);
        LV_ASSERT(dash_settings != 0);
        // Write it out to a file
        if (lv_fs_open(&fp, xml_settings_path, LV_FS_MODE_WR) == LV_FS_RES_OK)
        {
            // Write out, dont care if it fails really.
            lv_fs_write(&fp, xml_raw, strlen(xml_default), &bw);
            lv_fs_close(&fp);
        }
    }

    // Read in other settings from the xml file.
    char settings_value[32];
    struct xml_node *settings_node;
    settings_node = xml_easy_child(dash_settings, (const uint8_t *)"use_fahrenheit", 0);
    if (settings_node != 0)
    {
        xml_string_copy(xml_node_content(settings_node), (uint8_t *)settings_value, 1);
        settings_use_fahrenheit = LV_MIN(1, settings_value[0] - '0');
        nano_debug(LEVEL_TRACE, "TRACE: settings_use_fahrenheit is %d\n", settings_use_fahrenheit);
    }
    settings_node = xml_easy_child(dash_settings, (const uint8_t *)"default_screen_index", 0);
    if (settings_node != 0)
    {
        xml_string_copy(xml_node_content(settings_node), (uint8_t *)settings_value, 1);
        settings_default_screen_index = LV_MIN(xml_node_children(dash_pages) - 1, settings_value[0] - '0');
        nano_debug(LEVEL_TRACE, "TRACE: settings_default_screen_index is %d\n", settings_default_screen_index);
    }
    settings_node = xml_easy_child(dash_settings, (const uint8_t *)"auto_launch_dvd", 0);
    if (settings_node != 0)
    {
        xml_string_copy(xml_node_content(settings_node), (uint8_t *)settings_value, 1);
        settings_auto_launch_dvd = LV_MIN(1, settings_value[0] - '0');
        nano_debug(LEVEL_TRACE, "TRACE: settings_auto_launch_dvd is %d\n", settings_auto_launch_dvd);
    }

    // Get the overall screen size for later use
    lv_coord_t w = lv_obj_get_width(ROOT_PARENT);
    lv_coord_t h = lv_obj_get_height(ROOT_PARENT);

    // Setup input devices and a default input group
    static lv_indev_t *indev;
    static lv_group_t *input_group;
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

    // Init other parts of the dash
    jpeg_decoder_init(LV_COLOR_DEPTH);
    dash_styles_init();
    titlelist_init();
    synop_menu_init();
    main_menu_init();

    // Create a gradient background
    lv_obj_t *grad = lv_obj_create(ROOT_PARENT);
    lv_obj_add_style(grad, &dash_background_style, LV_PART_MAIN);
    lv_obj_set_size(grad, w, h);
    lv_obj_center(grad);

    // Create a tileview object to manage different pages
    page_tiles = lv_tileview_create(ROOT_PARENT);
    lv_obj_align(page_tiles, LV_ALIGN_TOP_MID, DASH_XMARGIN, DASH_YMARGIN);
    lv_obj_set_size(page_tiles, w - DASH_XMARGIN, h - 2 * DASH_YMARGIN);
    lv_obj_set_style_bg_opa(page_tiles, 0, LV_PART_MAIN);
    lv_obj_clear_flag(page_tiles, LV_OBJ_FLAG_SCROLLABLE);

    // Create a footer label which shows the current item name
    label_footer = lv_label_create(ROOT_PARENT);
    lv_obj_align(label_footer, LV_ALIGN_BOTTOM_MID, 0, -DASH_YMARGIN);
    lv_obj_add_style(label_footer, &titleview_header_footer_style, LV_PART_MAIN);
    lv_label_set_text(label_footer, "");
    lv_obj_update_layout(label_footer);

    // Work out how many pages we need from xml file
    parser_cnt = xml_node_children(dash_pages);
    if (parser_cnt > DASH_MAX_PAGES)
    {
        parser_cnt = DASH_MAX_PAGES;
    }

    // Setup a item parser and a viewable image scroller for each page.
    lv_memset(parsers, 0, sizeof(parse_handle_t) * parser_cnt);
    recent_parser = NULL;
    parser_current = NULL;
    for (int i = 0; i < parser_cnt; i++)
    {
        // Add the tile to create a new page
        parsers[i].tile = lv_tileview_add_tile(page_tiles, i, 0, LV_DIR_NONE);

        // Create a container that will have our scrolling game art
        parsers[i].scroller = lv_obj_create(parsers[i].tile);

        // Register the scroller container with our input driver. lv_obj dont have input callbacks by default.
        lv_obj_add_event_cb(parsers[i].scroller, input_callback, LV_EVENT_KEY, &parsers[i]);
        lv_group_add_obj(lv_group_get_default(), parsers[i].scroller);

        parse_handle_t *parser = &parsers[i];
        lv_obj_t *tile = parsers[i].tile;
        lv_obj_t *scroller = parsers[i].scroller;

        // Create a header label for the page from the xml
        char title[32];
        lv_obj_t *label_page_title = lv_label_create(tile);
        struct xml_node *page = xml_node_child(dash_pages, i);
        struct xml_string *page_title = xml_node_name(page);
        xml_string_copy(page_title, (uint8_t *)title, xml_string_length(page_title));
        title[xml_string_length(page_title)] = '\0';
        lv_label_set_text(label_page_title, title);
        lv_obj_align(label_page_title, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_style(label_page_title, &titleview_header_footer_style, LV_PART_MAIN);
        lv_obj_update_layout(label_page_title);

        // Setup the container for our scrolling game art
        int sc_parent_w = lv_obj_get_width(lv_obj_get_parent(scroller));
        int sc_parent_h = lv_obj_get_height(lv_obj_get_parent(scroller));
        // Make the width exactly equal to the highest number of thumbnails that can fit
        int sc_w = sc_parent_w - (sc_parent_w % DASH_THUMBNAIL_WIDTH);
        int sc_h = sc_parent_h - lv_obj_get_height(label_page_title) - lv_obj_get_height(label_footer);
        lv_obj_add_style(scroller, &titleview_style, LV_PART_MAIN);
        lv_obj_align(scroller, LV_ALIGN_TOP_MID, 0, lv_obj_get_height(label_page_title));
        lv_obj_set_width(scroller, sc_w);
        lv_obj_set_height(scroller, sc_h);
        // Use flex layout, so new titles automatically get positioned nicely.
        lv_obj_set_layout(scroller, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_update_layout(scroller);

        // Each scrolling window may have multiple search directories.
        // Parse from xml and add them to our search list
        size_t parse_directories = xml_node_children(page);
        if (parse_directories > DASH_MAX_PATHS_PER_PAGE)
        {
            nano_debug(LEVEL_WARN, "WARN: Found %d parse directories on page %s, but capped to %d\n",
                       parse_directories, title, DASH_MAX_PATHS_PER_PAGE);
            parse_directories = DASH_MAX_PATHS_PER_PAGE;
        }
        nano_debug(LEVEL_TRACE, "TRACE: Found %d parse directories on page  %s\n", parse_directories, title);
        parser->scroller = scroller;
        parser->dir_handle.drv = NULL;
        parser->num_paths = parse_directories;
        parser->current_path_index = 0;
        for (size_t j = 0; j < parse_directories; j++)
        {
            struct xml_string *dir_name = xml_node_content(xml_node_child(page, j));
            size_t dir_len = LV_MIN(xml_string_length(dir_name), DASH_MAX_PATHLEN);
            parser->paths[j] = (char *)lv_mem_alloc(dir_len);
            xml_string_copy(dir_name, (uint8_t *)parser->paths[j], dir_len);
            parser->paths[j][dir_len] = '\0';
            nano_debug(LEVEL_TRACE, "TRACE: Found path %s for page %s\n", parser->paths[j], title);
            // If the xml contains the word 'Recent' as a node, we add a recent page on the first instance.
            if (recent_parser == NULL && (strcmp("Recent", parser->paths[j]) == 0 || strcmp("recent", parser->paths[j]) == 0))
            {
                nano_debug(LEVEL_TRACE, "TRACE: Found \"Recent\" in xml. Enabling recent page\n");
                recent_parser = parser;
            }
        }
    }

    // If the recent items page is enabled, read in RECENT_TITLES and populate the scroller
    if (recent_parser != NULL)
    {
        // Read recent items file set by RECENT_TITLES
        int recent_title_cnt = 0;
        recent_titles = NULL;
        char *data = (char *)lv_fs_orc(RECENT_TITLES, &fs);
        if (data != NULL)
        {
            nano_debug(LEVEL_TRACE, "TRACE: Found %s. Reading recent items\n", RECENT_TITLES);
            // Find number of lines and replace with string terminators
            for (uint32_t i = 0; i < fs; i++)
            {
                if (data[i] == '\n')
                {
                    recent_title_cnt++;
                    data[i] = '\0';
                }
            }
            recent_titles = (char **)lv_mem_alloc(sizeof(char *) * recent_title_cnt);
            // Add all the recent titles to the recent items page
            for (int i = 0; i < recent_title_cnt; i++)
            {
                title_t *new_title = &recent_parser->title[lv_obj_get_child_cnt(recent_parser->scroller)];
                char *launch_folder = (i == 0) ? data : strchr(launch_folder, '\0') + 1;
                recent_titles[i] = launch_folder;
                if (titlelist_add(new_title, launch_folder, recent_parser->scroller) != 0)
                {
                    nano_debug(LEVEL_TRACE, "TRACE: Found recent item %s in %s\n", new_title->title, RECENT_TITLES);
                    lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_KEY, recent_parser);
                    lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_FOCUSED, recent_parser);
                    lv_obj_add_event_cb(new_title->image_container, input_callback, LV_EVENT_DEFOCUSED, recent_parser);
                }
            }
        }
        else
        {
            nano_debug(LEVEL_WARN, "WARN: Could not find %s. Not recent items added\n", RECENT_TITLES);
        }
    }

    // Start the normal title parsing thread
    nano_debug(LEVEL_TRACE, "TRACE: Starting parser thread\n");
    parser_thread = SDL_CreateThread(game_parser_thread, "game_parser_thread", (void *)NULL);

    // Set the initial page as per the xml file settings.
    change_page(settings_default_screen_index);

#ifdef NXDK
#ifdef DBG
    lv_obj_t *mem_label = lv_label_create(ROOT_PARENT);
    lv_obj_set_style_bg_color(mem_label, lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_text_color(mem_label, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_timer_create(mem_callback, 500, mem_label);
    lv_obj_align(mem_label, LV_ALIGN_BOTTOM_LEFT, 150, 0);
#endif
#endif
    nano_debug(LEVEL_TRACE, "TRACE: Dash init compete\n");
}

void dash_deinit(void)
{
    dash_running = false;
    SDL_WaitThread(parser_thread, NULL);
    jpeg_decoder_deinit();
    //All threads are stopped. Dont need lvgl locks anymore.
    if (dash_config)
    {
        xml_document_free(dash_config, false);
        lv_mem_free(xml_raw);
    }
    titlelist_deinit();
    main_menu_deinit();
    synop_menu_deinit();
    dash_styles_deinit();
}

void dash_clear_recent_list(void)
{
    lv_fs_file_t fp;
    uint32_t bw;
    // Check recent page is enabled
    if (recent_parser == NULL)
    {
        return;
    }
    // Delete all the image containers on the recent items page
    lv_obj_t *child = lv_obj_get_child(recent_parser->scroller, 0);
    while (child)
    {
        child = lv_obj_get_child(recent_parser->scroller, 0);
        if (child != NULL)
        {
            title_t *title = (title_t *)child->user_data;
            titlelist_remove(title);
        }
    }
    // Clean up some variables
    if (recent_titles != NULL)
    {
        lv_mem_free(recent_titles);
        recent_titles = NULL;
    }
    // Reset the recent items file
    if (lv_fs_open(&fp, RECENT_TITLES, LV_FS_MODE_WR) == LV_FS_RES_OK)
    {
        lv_fs_write(&fp, "", 1, &bw);
        lv_fs_close(&fp);
    }
    // Refresh the current page to update changes
    change_page(current_tile_index);
}
