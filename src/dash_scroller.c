// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"
#include <src/misc/lv_lru.h>
#include <src/misc/lv_ll.h>

parse_handle_t *parsers[DASH_MAX_PAGES];
static lv_obj_t *page_tiles;
static lv_obj_t *label_footer;
static int page_current;
static lv_lru_t *thumbnail_cache;
static size_t thumbnail_cache_size = (16 * 1024 * 1024);

#ifdef NXDK
#define JPEG_BPP (2)
#else
#define JPEG_BPP (4)
#endif

typedef struct
{
    lv_obj_t *image_container;
} jpeg_ll_value_t;
static lv_ll_t jpeg_decomp_list;

void dash_scroller_set_page()
{
    toml_array_t *pages = toml_array_in(dash_search_paths, "pages");
    int page_max = LV_MIN(toml_array_nelem(pages), DASH_MAX_PAGES);
    page_current = LV_CLAMP(0, page_current, page_max - 1);
    lv_obj_set_tile_id(page_tiles, page_current, 0, LV_ANIM_ON);
    lv_obj_t *tile = lv_tileview_get_tile_act(page_tiles);
    lv_obj_t *scroller = lv_obj_get_child(tile, 0);
    int *current_index = (int *)&scroller->user_data;
    *current_index = LV_MAX(1, *current_index);

    assert(lv_obj_get_child_cnt(scroller) >= 1);

    lv_obj_t *focus_item = lv_obj_get_child(scroller, *current_index);
    if (lv_obj_is_valid(focus_item) == false)
    {
        // We were are trying to focus an invalid object, revert to index 0 which should always
        // be present
        *current_index = 0;
        focus_item = lv_obj_get_child(scroller, *current_index);
        assert(lv_obj_is_valid(scroller));
    }

    dash_focus_set_final(lv_obj_get_child(scroller, 0));
    dash_focus_change(focus_item);
}

static void jpg_decompression_complete_cb(void *img, void *mem, int w, int h, void *user_data)
{
    lv_obj_t *image_container = user_data;
    title_t *t = image_container->user_data;

    if (img == NULL)
    {
        t->jpg_info->decomp_handle = NULL;
        return;
    }

    lvgl_getlock();

    t->jpg_info->mem = mem;
    t->jpg_info->image = img;
    t->jpg_info->w = w;
    t->jpg_info->h = h;
    t->jpg_info->decomp_handle = NULL;

    t->jpg_info->canvas = lv_canvas_create(image_container);

    lv_img_cf_t cf = LV_IMG_CF_TRUE_COLOR;
    assert(JPEG_BPP == 2 || JPEG_BPP == 4);
    if (JPEG_BPP * 8 != LV_COLOR_DEPTH)
    {
        cf = (JPEG_BPP == 2) ? LV_IMG_CF_RGB565 : LV_IMG_CF_RGBA8888;
    }

    lv_canvas_set_buffer(t->jpg_info->canvas, img, w, h, cf);

    lv_img_set_size_mode(t->jpg_info->canvas, LV_IMG_SIZE_MODE_REAL);
    lv_img_set_zoom(t->jpg_info->canvas, DASH_THUMBNAIL_WIDTH * 256 / w);
    lv_obj_mark_layout_as_dirty(t->jpg_info->canvas);

    lv_lru_set(thumbnail_cache, &image_container, sizeof(lv_obj_t *), t, w * h * JPEG_BPP);
    lvgl_removelock();
}

static void update_thumbnail_callback(lv_event_t *event)
{
    lv_obj_t *image_container = lv_event_get_target(event);
    title_t *t = image_container->user_data;

    if (t->jpg_info == NULL)
    {
        return;
    }

    if (t->jpg_info->decomp_handle == NULL && t->jpg_info->mem == NULL)
    {
        t->jpg_info->decomp_handle = jpeg_decoder_queue(t->jpg_info->thumb_path,
                                                        jpg_decompression_complete_cb, image_container);
        jpeg_ll_value_t *n = _lv_ll_ins_tail(&jpeg_decomp_list);
        n->image_container = image_container;
    }
}

static int get_launch_path_callback(void *param, int argc, char **argv, char **azColName)
{
    (void) param;
    (void) azColName;
    (void) argc;
    assert(argc == 1);

    strncpy(dash_launch_path, argv[0], DASH_MAX_PATH);
    return 0;
}

static void item_selection_callback(lv_event_t *event)
{
    lv_event_code_t e = lv_event_get_code(event);

    lv_obj_t *item_container = lv_event_get_target(event);
    title_t *t = item_container->user_data;
    if (e == LV_EVENT_FOCUSED || e == LV_EVENT_DEFOCUSED)
    {
        lv_style_value_t border_width;
        lv_style_value_t border_colour;
        lv_style_get_prop(&titleview_image_container_style, LV_STYLE_BORDER_WIDTH, &border_width);
        lv_style_get_prop(&titleview_image_container_style, LV_STYLE_BORDER_COLOR, &border_colour);
        if (e == LV_EVENT_FOCUSED)
        {
            lv_obj_set_style_border_width(item_container, border_width.num * 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(item_container, lv_color_white(), LV_PART_MAIN);
            lv_label_set_text(label_footer, t->title);
        }
        else
        {
            lv_obj_set_style_border_width(item_container, border_width.num, LV_PART_MAIN);
            lv_obj_set_style_border_color(item_container, border_colour.color, LV_PART_MAIN);
        }
    }
    else if (e == LV_EVENT_KEY)
    {
        lv_obj_t *scroller = lv_obj_get_parent(item_container);
        int *current_index = (int *)&scroller->user_data;

        lv_key_t key = *((lv_key_t *)lv_event_get_param(event));
        if (key == DASH_PREV_PAGE || key == DASH_NEXT_PAGE)
        {
            page_current += (key == DASH_NEXT_PAGE) ? (1) : -1;
            dash_scroller_set_page();
        }
        // L and R are the back triggers
        else if (key == LV_KEY_RIGHT || key == LV_KEY_LEFT || key == LV_KEY_UP || key == LV_KEY_DOWN || key == 'L' || key == 'R')
        {
            int last_index = lv_obj_get_child_cnt(scroller) - 1;
            int new_index = *current_index;

            // FIXME: Should really allow thumbnails of any width
            int tiles_per_row = lv_obj_get_width(scroller) / DASH_THUMBNAIL_WIDTH;

            // At the start, loop to end
            if (*current_index <= 1 && key == LV_KEY_UP)
            {
                new_index = last_index;
            }
            // At the end, loop to start
            else if (*current_index == last_index && key == LV_KEY_DOWN)
            {
                new_index = 1;
            }
            // Increment left or right one
            else if (key == LV_KEY_RIGHT || key == LV_KEY_LEFT)
            {
                new_index += (key == LV_KEY_RIGHT) ? 1 : -1;
            }
            // Increment up or down one
            else if (key == LV_KEY_UP || key == LV_KEY_DOWN)
            {
                if (new_index == 0)
                {
                    new_index = 1;
                }
                else
                {
                    new_index += (key == LV_KEY_DOWN) ? tiles_per_row : -tiles_per_row;
                }
            }
            // Increment up or down lots (LT and RT)
            else if (key == 'L' || key == 'R')
            {
                new_index += (key == 'R') ? (tiles_per_row * 8) : -(tiles_per_row * 8);
            }
            // Clamp the new index within the limits. Prefer index 1 as index 0 is the null
            // item. But will revert to 0 if no items in page
            new_index = LV_MAX(1, new_index);
            new_index = LV_CLAMP(0, new_index, last_index);
            *current_index = new_index;

            lv_obj_t *new_item_container = lv_obj_get_child(scroller, new_index);
            assert(lv_obj_is_valid(new_item_container));

            // Scroll until our new selection is in view
            lv_obj_scroll_to_view_recursive(new_item_container, LV_ANIM_ON);
            dash_focus_change(new_item_container);

            int s = LV_MAX(1, new_index - 2 * tiles_per_row) * (key == LV_KEY_UP);
            int e = LV_MIN(last_index, new_index + 2 * tiles_per_row) * (key == LV_KEY_DOWN);
            for (int i = s; i < new_index + e; i++)
            {
                lv_event_t e;
                e.target = lv_obj_get_child(scroller, i);
                if (e.target)
                {
                    title_t *t = e.target->user_data;
                    if (t->jpg_info)
                    {
                        t->jpg_info->prevent_abort = true;
                        update_thumbnail_callback(&e);
                    }
                }
            }
        }
        else if (key == DASH_INFO_PAGE && *current_index != 0)
        {
            dash_synop_open((t->db_id));
        }
        else if (key == DASH_SETTINGS_PAGE)
        {
            dash_mainmenu_open();
        }
        else if (key == LV_KEY_ENTER && *current_index > 0)
        {
            char cmd[SQL_MAX_COMMAND_LEN];
            char time_str[20];
            lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_GET_LAUNCH_PATH, t->db_id);
            db_command_with_callback(cmd, get_launch_path_callback, NULL);

            if (dash_launcher_is_launchable(dash_launch_path))
            {
                platform_get_iso8601_time(time_str);
                lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_SET_LAST_LAUNCH_DATETIME, time_str, t->db_id);
                db_command_with_callback(cmd, NULL, NULL);

                lv_set_quit(LV_QUIT_OTHER);
            }
        }
    }
}

static void item_deletion_callback(lv_event_t *event)
{
    lv_obj_t *item_container = lv_event_get_target(event);
    title_t *t = item_container->user_data;
    if (t->jpg_info)
    {
        t->jpg_info->decomp_handle = NULL;
        lv_mem_free(t->jpg_info->thumb_path);
        lv_mem_free(t->jpg_info);
        lv_mem_free(t);
    }
}

typedef struct item_strings
{
    char id[8];
    char title[MAX_META_LEN];
    char *launch_path;
    lv_obj_t *item_container;
    struct item_strings *next;
} item_strings_t;

typedef struct item_strings_callback
{
    item_strings_t *head;
    item_strings_t *tail;
} item_strings_callback_t;

// Callback when a new row is read from the SQL database. This is a new item to add
static int item_scan_callback(void *param, int argc, char **argv, char **azColName)
{
    item_strings_callback_t *item_cb = param;
    (void) argc;
    (void) azColName;

    item_strings_t *item = lv_mem_alloc(sizeof(item_strings_t));
    lv_memset(item, 0, sizeof(item_strings_t));

    assert(strcmp(azColName[0], SQL_TITLE_DB_ID) == 0);
    assert(strcmp(azColName[1], SQL_TITLE_NAME) == 0);
    assert(strcmp(azColName[2], SQL_TITLE_LAUNCH_PATH) == 0);

    strncpy(item->id, argv[0], sizeof(item->id) - 1);
    strncpy(item->title, argv[1], sizeof(item->title) - 1);

    if (strlen(argv[2]) <= 3)
    {
        lv_mem_free(item);
        return 0;
    }

    item->launch_path = lv_strdup(argv[2]);

    if (item_cb->tail == NULL)
    {
        assert(item_cb->head == NULL);
        item_cb->head = item;
        item_cb->tail = item;
    }
    else
    {
        item_cb->tail->next = item;
        item_cb->tail = item;
    }
    return 0;
}

static void item_scan_add(lv_obj_t *scroller, item_strings_callback_t *item_cb)
{
    item_strings_t *item = item_cb->head;
    // First we add all the items to lvgl so they display quickly
    while (item)
    {
        title_t *t = lv_mem_alloc(sizeof(title_t));
        assert(t);
        if (t == NULL)
        {
            continue;
        }
        t->jpg_info = NULL;
        t->title[0] = '\0';
        t->db_id = atoi(item->id);

        lvgl_getlock();
        lv_obj_t *item_container = lv_obj_create(scroller);
        item->item_container = item_container;
        item_container->user_data = t;
        lv_obj_add_style(item_container, &titleview_image_container_style, LV_PART_MAIN);
        lv_obj_clear_flag(item_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_height(item_container, DASH_THUMBNAIL_HEIGHT);
        lv_obj_set_width(item_container, DASH_THUMBNAIL_WIDTH);

        // Create a label with the game title
        lv_obj_t *label = lv_label_create(item_container);
        lv_obj_add_style(label, &titleview_image_text_style, LV_PART_MAIN);
        lv_obj_set_width(label, DASH_THUMBNAIL_WIDTH);
        lv_obj_update_layout(label);
        lv_label_set_text(label, item->title);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

        lv_group_add_obj(lv_group_get_default(), item_container);
        lv_obj_add_event_cb(item_container, item_selection_callback, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(item_container, item_selection_callback, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(item_container, item_selection_callback, LV_EVENT_DEFOCUSED, NULL);
        lv_obj_add_event_cb(item_container, item_deletion_callback, LV_EVENT_DELETE, NULL);

        strncpy(t->title, item->title, sizeof(t->title) - 1);
        lvgl_removelock();
        item = item->next;
    }

    // Next we scan for thumbnails
    char *thumb_path = lv_mem_alloc(DASH_MAX_PATH);
    item = item_cb->head;
    while (item)
    {
        lv_obj_t *item_container = item->item_container;
        title_t *t = item_container->user_data;

        // Check if a thumbnail exists
        strcpy(thumb_path, item->launch_path);
        lv_mem_free(item->launch_path);
        char *b = strrchr(thumb_path, '\\');
        if  (b == NULL) b = strrchr(thumb_path, '/');
        if (b == NULL)
        {
            item = item->next;
            continue;
        }
        if (b)
        {
            strcpy(&b[1], DASH_GAME_THUMBNAIL);
        }
        DWORD fileAttributes = GetFileAttributes(thumb_path);
        if (fileAttributes == INVALID_FILE_ATTRIBUTES || (fileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            item = item->next;
            continue;
        }

        jpg_info_t *jpg_info = lv_mem_alloc(sizeof(jpg_info_t));
        assert(jpg_info);
        if (jpg_info == NULL)
        {
            item = item->next;
            continue;
        }
        lv_memset(jpg_info, 0, sizeof(jpg_info_t));
        jpg_info->thumb_path = lv_strdup(thumb_path);

        lvgl_getlock();
        t->jpg_info = jpg_info;
        lv_obj_add_event_cb(item_container, update_thumbnail_callback, LV_EVENT_DRAW_MAIN_END, NULL);
        lv_obj_invalidate(item_container);
        lvgl_removelock();
        item = item->next;
    }
    lv_mem_free(thumb_path);
}

static void dash_scroller_get_sort_strings(unsigned int sort_index, const char **sort_by, const char **order_by)
{
    switch (sort_index)
    {
    case DASH_SORT_RATING:
        *sort_by = SQL_TITLE_RATING;
        *order_by = "DESC";
        break;
    case DASH_SORT_LAST_LAUNCH:
        *sort_by = SQL_TITLE_LAST_LAUNCH;
        *order_by = "DESC";
        break;
    case DASH_SORT_RELEASE_DATE:
        *sort_by = SQL_TITLE_RELEASE_DATE;
        *order_by = "DESC";
        break;
    default:
        *sort_by = SQL_TITLE_NAME;
        *order_by = "ASC";
    }
}

static int db_scan_thread_f(void *param)
{
    parse_handle_t *p = param;
    char cmd[SQL_MAX_COMMAND_LEN];
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);

    item_strings_callback_t item_cb;
    lv_memset(&item_cb, 0, sizeof(item_strings_callback_t));

    if (strcmp(p->page_title, "Recent") == 0)
    {
        lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_GET_RECENT,
                        SQL_TITLE_DB_ID "," SQL_TITLE_NAME "," SQL_TITLE_LAUNCH_PATH,
                        dash_settings.earliest_recent_date, dash_settings.max_recent_items);

        db_command_with_callback(cmd, item_scan_callback, &item_cb);
        item_scan_add(p->scroller, &item_cb);
    }
    else
    {
        int sort_index = 0;
        const char *sort_by;
        const char *order_by;
        dash_scroller_get_sort_value(p->page_title, &sort_index);
        dash_scroller_get_sort_strings(sort_index, &sort_by, &order_by);

        lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_GET_SORTED_LIST,
                            SQL_TITLE_DB_ID "," SQL_TITLE_NAME "," SQL_TITLE_LAUNCH_PATH,
                            p->page_title, sort_by, order_by);

        db_command_with_callback(cmd, item_scan_callback, &item_cb);
        item_scan_add(p->scroller, &item_cb);
    }

    while (item_cb.head)
    {
        item_strings_t *next_item = item_cb.head->next;
        lv_mem_free(item_cb.head);
        item_cb.head = next_item;
    }
    return 0;
}

static void cache_free(title_t *t)
{
    assert(lv_obj_is_valid(t->jpg_info->canvas));
    jpeg_decoder_abort(t->jpg_info->decomp_handle);
    assert(t->jpg_info->mem);
    free(t->jpg_info->mem);
    lv_obj_del(t->jpg_info->canvas);
    t->jpg_info->decomp_handle = NULL;
    t->jpg_info->mem = NULL;
    t->jpg_info->image = NULL;
}

void dash_scroller_clear_page(const char *page_title)
{
    lv_obj_t *scroller = NULL;
    for (int i = 0; i < DASH_MAX_PAGES; i++)
    {
        if (parsers[i] == NULL)
        {
            continue;
        }
        if (strcmp(page_title, parsers[i]->page_title) == 0)
        {
            scroller = parsers[i]->scroller;
            lv_obj_t *item_container = lv_obj_get_child(scroller, 1);
            while (item_container)
            {
                lv_obj_del(item_container);
                item_container = lv_obj_get_child(scroller, 1);
            }
        }
    }
}

static void jpeg_clear_timer(lv_timer_t *t)
{
    (void) t;
    jpeg_ll_value_t *item = _lv_ll_get_head(&jpeg_decomp_list);
    while (item)
    {
        lv_obj_t *image_container = item->image_container;
        title_t *title = image_container->user_data;
        assert(title->jpg_info);
        // Still decompressing but no longer visible. Lets abort it.
        if (lv_obj_is_visible(image_container) == false && title->jpg_info->prevent_abort == false)
        {
            jpeg_decoder_abort(title->jpg_info->decomp_handle);
            title->jpg_info->decomp_handle = NULL;
        }
        // Jpeg finished decomp (or was aborted already), dont need to it anymore
        if (title->jpg_info->decomp_handle == NULL)
        {
            _lv_ll_remove(&jpeg_decomp_list, item);
            lv_mem_free(item);
            item = _lv_ll_get_head(&jpeg_decomp_list);
        }
        // Still compressing, but still visible. Try next item
        else
        {
            item = _lv_ll_get_next(&jpeg_decomp_list, item);
        }
    }
}

void dash_scroller_init()
{
    lv_coord_t w = lv_obj_get_width(lv_scr_act());
    lv_coord_t h = lv_obj_get_height(lv_scr_act());
    page_current = dash_settings.startup_page_index;

    lv_memset(parsers, 0, sizeof(parsers));
    thumbnail_cache = lv_lru_create(thumbnail_cache_size, 175 * 248 * JPEG_BPP,
                                    (lv_lru_free_t *)cache_free, NULL);

    jpeg_decoder_init(JPEG_BPP * 8, 256);

    _lv_ll_init(&jpeg_decomp_list, sizeof(jpeg_ll_value_t));
    lv_timer_create(jpeg_clear_timer, LV_DISP_DEF_REFR_PERIOD, NULL);
 
    // Create a tileview object to manage different pages
    page_tiles = lv_tileview_create(lv_scr_act());
    lv_obj_align(page_tiles, LV_ALIGN_TOP_MID, 0, DASH_YMARGIN);
    lv_obj_set_size(page_tiles, w - (2 * DASH_XMARGIN), h - (2 * DASH_YMARGIN));
    lv_obj_set_style_bg_opa(page_tiles, 0, LV_PART_MAIN);
    lv_obj_clear_flag(page_tiles, LV_OBJ_FLAG_SCROLLABLE);

    // Create a footer label which shows the current item name
    label_footer = lv_label_create(lv_scr_act());
    lv_obj_align(label_footer, LV_ALIGN_BOTTOM_MID, 0, -DASH_YMARGIN);
    lv_obj_add_style(label_footer, &titleview_header_footer_style, LV_PART_MAIN);
    lv_label_set_text(label_footer, "");
    lv_obj_update_layout(label_footer);
}

void dash_scroller_scan_db()
{
    toml_table_t *paths = dash_search_paths;
    toml_array_t *pages = toml_array_in(paths, "pages");
    int dash_num_pages = LV_MIN(toml_array_nelem(pages), DASH_MAX_PAGES);

    lv_obj_clean(page_tiles);
    for (int i = 0; i < DASH_MAX_PAGES; i++)
    {
        parse_handle_t *parser = parsers[i];
        if (parser == NULL)
            continue;
        lv_mem_free(parser);
        parsers[i] = NULL;
    }

    // Create a parser object for each page. This includes a container to show our game art
    // and all the parse directories to find our items
    for (int i = 0; i < dash_num_pages; i++)
    {
        parse_handle_t *parser = lv_mem_alloc(sizeof(parse_handle_t));
        assert(parser);
        parsers[i] = parser;
        lv_obj_t **tile = &parser->tile;
        lv_obj_t **scroller = &parser->scroller;
        lv_memset(parser, 0, sizeof(parse_handle_t));

        // Create a new page in our tileview
        *tile = lv_tileview_add_tile(page_tiles, i, 0, LV_DIR_NONE);

        // Create a container that will have our scroller game art
        *scroller = lv_obj_create(*tile);
        (*scroller)->user_data = (void *)1; // Store the currently active index here

        // Create a header label for the page from the xml
        lv_obj_t *label_page_title = lv_label_create(*tile);
        toml_datum_t name_str = toml_string_in(toml_table_at(pages, i), "name");
        if (name_str.ok)
        {
            strncpy(parser->page_title, name_str.u.s, sizeof(parser->page_title) - 1);
        }
        lv_label_set_text(label_page_title, parser->page_title);
        lv_obj_align(label_page_title, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_style(label_page_title, &titleview_header_footer_style, LV_PART_MAIN);
        lv_obj_update_layout(label_page_title);

        // Setup the container for our scrolling game art
        int sc_parent_w = lv_obj_get_width(lv_obj_get_parent(*scroller));
        int sc_parent_h = lv_obj_get_height(lv_obj_get_parent(*scroller));
        // Make the width exactly equal to the highest number of thumbnails that can fit
        int sc_w = sc_parent_w - (sc_parent_w % DASH_THUMBNAIL_WIDTH);
        int sc_h = sc_parent_h - lv_obj_get_height(label_page_title) - lv_obj_get_height(label_footer);
        lv_obj_add_style(*scroller, &titleview_style, LV_PART_MAIN);
        lv_obj_align(*scroller, LV_ALIGN_TOP_MID, 0, lv_obj_get_height(label_page_title));
        lv_obj_set_width(*scroller, sc_w);
        lv_obj_set_height(*scroller, sc_h);
        // Use flex layout, so new titles automatically get positioned nicely.
        lv_obj_set_layout(*scroller, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(*scroller, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_update_layout(*scroller);

        // Create atleast ONE item in the scroller. This is just a null item when nothing is present
        title_t *t = lv_mem_alloc(sizeof(title_t));
        t->db_id = -1;
        strcpy(t->title, "No item selected");
        lv_obj_t *null_item = lv_obj_create(*scroller);
        null_item->user_data = t;
        lv_obj_add_flag(null_item, LV_OBJ_FLAG_HIDDEN);
        lv_group_add_obj(lv_group_get_default(), null_item);
        lv_obj_add_event_cb(null_item, item_selection_callback, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(null_item, item_selection_callback, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(null_item, item_selection_callback, LV_EVENT_DEFOCUSED, NULL);

        // Start a thread that starts reading the database for items on this page.
        // Thread needs to have a mutex on the database and lvgl
        parser->db_scan_thread = SDL_CreateThread(db_scan_thread_f, "game_parser_thread", parser);
    }
}

const char *dash_scroller_get_title(int index)
{
    if (index >= DASH_MAX_PAGES)
    {
        return NULL;
    }
    if (parsers[index] == NULL)
    {
        return NULL;
    }
    return parsers[index]->page_title;
}

int dash_scroller_get_page_count()
{
    return lv_obj_get_child_cnt(page_tiles);
}

bool dash_scroller_get_sort_value(const char *page_title, int *sort_value)
{
    if (page_title == NULL || sort_value == NULL)
    {
        return false;
    }

    const char* valueStart = strstr(dash_settings.sort_strings, page_title);
    if (valueStart == NULL)
    {
        return false;
    }

    // Move past the parameter name and '=' sign
    valueStart += strlen(page_title) + 1;

    int value;
    if (sscanf(valueStart, "%d", &value) != 1)
    {
        return false;
    }

    value = LV_CLAMP(0, value, DASH_SORT_MAX - 1);  
    *sort_value = value;
    return true;
}

struct resort_param
{
    int sort_index;
    lv_obj_t **sorted_objs;
};

static int resort_page_callback(void *param, int argc, char **argv, char **azColName)
{
    (void) param;
    (void) azColName;
    (void) argc;

    assert(argc == 1);

    struct resort_param *p = param;
    lv_obj_t *scroller = p->sorted_objs[0];
    int db_id = atoi(argv[0]);
    lv_task_handler();
    for (unsigned int i = 1; i < lv_obj_get_child_cnt(scroller); i++)
    {
        lv_obj_t *item_container = lv_obj_get_child(scroller, i);
        //assert(lv_obj_is_valid(item_container));
        title_t *t = item_container->user_data;
        if (t->db_id == db_id)
        {
            p->sorted_objs[p->sort_index] = item_container;
            p->sort_index++;
            return 0;
        }
    }
    assert(0);
    return 0;
}

void dash_scroller_resort_page(const char *page_title)
{
    char cmd[SQL_MAX_COMMAND_LEN];
    int sort_index;
    if (dash_scroller_get_sort_value(page_title, &sort_index) == false)
    {
        return;
    }
    
    lv_obj_t *scroller = NULL;
    for (int i = 0; i < DASH_MAX_PAGES; i++)
    {
        if (parsers[i] == NULL)
        {
            continue;
        }
        if (strcmp(page_title, parsers[i]->page_title) == 0)
        {
            scroller = parsers[i]->scroller;
            break;
        }
    }
    assert(scroller);

    // If the scroller has no items leave. 1st item is a null item
    if (lv_obj_get_child_cnt(scroller) <= 1)
    {
        return;
    }

    const char *sort_by;
    const char *order_by;
    dash_scroller_get_sort_strings(sort_index, &sort_by, &order_by);

    lv_snprintf(cmd, sizeof(cmd), SQL_TITLE_GET_SORTED_LIST, SQL_TITLE_DB_ID, page_title, sort_by, order_by);

    int child_cnt = lv_obj_get_child_cnt(scroller);

    struct resort_param *p = lv_mem_alloc(sizeof(struct resort_param));
    p->sort_index = 1;
    p->sorted_objs = lv_mem_alloc(sizeof(lv_obj_t *) * child_cnt);
    
    lv_memset(p->sorted_objs, 0, sizeof(lv_obj_t *) * child_cnt);
    p->sorted_objs[0] = scroller;

    db_command_with_callback(cmd, resort_page_callback, p);
    for (int i = 1; i < child_cnt; i++)
    {
        scroller->spec_attr->children[i] = p->sorted_objs[i];
    }
    lv_mem_free(p->sorted_objs);
    lv_mem_free(p);
    lv_obj_mark_layout_as_dirty(scroller);
}
