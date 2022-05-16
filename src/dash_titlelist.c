// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lvgl/src/misc/lv_lru.h"
#include "dash.h"
#include "dash_styles.h"
#include "dash_titlelist.h"
#include "helpers/fileio.h"
#include "xml/src/xml.h"
#include "helpers/nano_debug.h"
#include "jpg_decoder.h"
#include <stdio.h>
#include <stdlib.h>

LV_IMG_DECLARE(default_tbn);

#define DELAYED_DECOMPRESS_PERIOD 50

static title_t *list_head = NULL;
static title_t *list_tail = NULL;

typedef struct
{
    title_t *title;
    void *src;
    uint32_t w;
    uint32_t h;
} draw_cache_value_t;
lv_lru_t *jpg_cache;
static int jpg_cache_size = 8 * 1024 * 1024;

// 1. jpg is onscreen, think about decompressing it soon.
static void jpg_on_screen_cb(lv_event_t *event);
// 2. Dont start decompression straight away, short delay to check its still on screen.
//    Helps prevent undeeded queuing when scrolling fast.
static void jpg_delayed_queue(lv_timer_t *timer);
// 3. Decompression is finished. Update image container.
static void jpg_decompression_complete_cb(void *buffer, int w, int h, void *user_data);

static void jpg_clean(title_t *title)
{
    if (title->jpeg_handle != NULL)
    {
        jpeg_decoder_abort(title->jpeg_handle);
        title->jpeg_handle = NULL;
    }
    if (title->thumb_jpeg != NULL)
    {
        lv_lru_remove(jpg_cache, &title, sizeof(title));
    }
}

static void jpg_decompression_complete_cb(void *buffer, int w, int h, void *user_data)
{
    title_t *title = (title_t *)user_data;
    nano_debug(LEVEL_TRACE, "TRACE: jpg_decompression_complete_cb %s\n", title->title);
    // If buffer == NULL. decompression was aborted or had an error.
    if (title->thumb_jpeg == NULL && buffer != NULL)
    {
        // This callback is from another thread, we need to obtain lvgl lock
        lvgl_getlock();
        title->thumb_jpeg = lv_canvas_create(title->image_container);
        title->thumb_jpeg->user_data = buffer;
        lv_canvas_set_buffer(title->thumb_jpeg, buffer, w, h, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(title->image_container, DASH_THUMBNAIL_WIDTH, DASH_THUMBNAIL_HEIGHT);
        lv_img_set_pivot(title->thumb_jpeg, 0, 0);
        lv_img_set_zoom(title->thumb_jpeg, DASH_THUMBNAIL_WIDTH * 256 / w);
        lv_obj_add_flag(title->thumb_default, LV_OBJ_FLAG_HIDDEN);

        // Add it to jpg cache
        draw_cache_value_t *jpg = lv_mem_alloc(sizeof(draw_cache_value_t));
        jpg->title = title;
        jpg->src = buffer;
        jpg->w = w;
        jpg->h = h;
        lv_lru_set(jpg_cache, &title, sizeof(title), jpg, w * h * sizeof(lv_color_t));

        lvgl_removelock();
    }
    // Done, we cant use this handle anymore. It is closed automatically by jpeg decoder.
    title->jpeg_handle = NULL;
}

static void jpg_delayed_queue(lv_timer_t *timer)
{
    title_t *title = (title_t *)timer->user_data;

    // If object is still on screen after delay, we queue the jpg decompression
    if (lv_obj_is_visible(title->image_container) == true)
    {
        draw_cache_value_t *jpg = NULL;
        lv_lru_get(jpg_cache, &title, sizeof(title), (void **)&jpg);
        if (jpg)
        {
            // Found in jpg cache, retrieve it complete cb
            jpg_decompression_complete_cb(jpg->src, jpg->w, jpg->h, title);
        }
        else
        {
            /*Drop the first two characters in the filename. These are lvgl specific*/
            int path_len = strlen(title->title_folder) + 1 + strlen(DASH_LAUNCH_EXE) + 1;
            char *thumbnail_path = (char *)lv_mem_alloc(path_len);
            lv_snprintf(thumbnail_path, path_len, "%s%c%s", title->title_folder, DASH_PATH_SEPARATOR, DASH_GAME_THUMBNAIL);
            title->jpeg_handle = jpeg_decoder_queue(&thumbnail_path[2], jpg_decompression_complete_cb, title);
            lv_mem_free(thumbnail_path);
        }
    }
    // We're done. The jpg is queued for decompression. Clean up
    lv_timer_del(timer);
    lv_obj_add_event_cb(title->image_container, jpg_on_screen_cb, LV_EVENT_DRAW_MAIN_BEGIN, title);
}

// Image container is on-screen. Don't begin jpeg decomp immediately, instead we wait DELAYED_DECOMPRESS_PERIOD then start
// This is incase we are scrolling by quickly, we can save queueing it completely.
static void jpg_on_screen_cb(lv_event_t *event)
{
    title_t *title = (title_t *)event->user_data;

    //Poke cache to refresh access count
    if (title->thumb_jpeg != NULL)
    {
        draw_cache_value_t *jpg = NULL;
        lv_lru_get(jpg_cache, &title, sizeof(title), (void **)&jpg);
    }

    if (title->thumb_jpeg == NULL && title->jpeg_handle == NULL && title->has_thumbnail == true)
    {
        lv_timer_create(jpg_delayed_queue, DELAYED_DECOMPRESS_PERIOD, title);
        lv_obj_remove_event_cb(title->image_container, jpg_on_screen_cb);
    }
}

// Extract title header and title certificate
static int title_parse(title_t *title)
{
    uint32_t br;
    lv_fs_file_t fp;
    int success = 1;
    char *launch_path = (char *)lv_mem_alloc(DASH_MAX_PATHLEN);
    do
    {
        lv_snprintf(launch_path, DASH_MAX_PATHLEN, "%s%c%s", title->title_folder, DASH_PATH_SEPARATOR, DASH_LAUNCH_EXE);
        if (lv_fs_open(&fp, launch_path, LV_FS_MODE_RD) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Could not open %s. Skipping title\n.", launch_path);
            success = 0;
            break;
        }

        if (lv_fs_read(&fp, &title->xbe_header, sizeof(xbe_header_t), &br) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Could not read title header for %s. Skipping title\n.", launch_path);
            success = 0;
            break;
        }
        if (strncmp((char *)&title->xbe_header.dwMagic, "XBEH", 4) != 0)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Invalid title header magic %08x. Skipping title\n.", title->xbe_header.dwMagic);
            success = 0;
            break;
        }

        uint32_t cert_addr = title->xbe_header.dwCertificateAddr - title->xbe_header.dwBaseAddr;
        if (lv_fs_seek(&fp, cert_addr, LV_FS_SEEK_SET) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Could not seek to offset %d for %s. Skipping title\n.", cert_addr, launch_path);
            success = 0;
            break;
        }
        if (lv_fs_read(&fp, &title->xbe_cert, sizeof(xbe_certificate_t), &br) != LV_FS_RES_OK)
        {
            nano_debug(LEVEL_ERROR, "ERROR: Could not read title certificate in %s. Skipping title\n.", launch_path);
            success = 0;
            break;
        }
    } while (0);

    lv_fs_close(&fp);
    lv_mem_free(launch_path);

    return success;
}

static void cache_free(draw_cache_value_t *jpg)
{
    lv_obj_del(jpg->title->thumb_jpeg);
    lv_obj_clear_flag(jpg->title->thumb_default, LV_OBJ_FLAG_HIDDEN);
    jpg->title->thumb_jpeg = NULL;
    free(jpg->src);
    lv_mem_free(jpg);
}

void titlelist_init(void)
{
    list_head = NULL;
    list_tail = NULL;
    jpg_cache = lv_lru_create(jpg_cache_size, 0x40000, (lv_lru_free_t *)cache_free, NULL);
}

void titlelist_deinit(void)
{
    title_t *title = list_head;
    while (title)
    {
        if (title->jpeg_handle != NULL)
        {
            jpeg_decoder_abort(title->jpeg_handle);
        }
        title = title->next;
    }
    list_head = NULL;
    list_tail = NULL;
    lv_lru_del(jpg_cache);
}

struct xml_string *title_get_synopsis(struct xml_document *title_xml, const char *node_name)
{
    struct xml_node *synopsis = xml_document_root(title_xml);
    struct xml_node *node = xml_easy_child(synopsis, (const uint8_t *)node_name, 0);
    if (node == 0)
    {
        return NULL;
    }
    struct xml_string *node_string = xml_node_content(node);
    if (node_string == NULL)
    {
        return NULL;
    }
    return node_string;
}

int titlelist_add(title_t *title, char *title_folder, lv_obj_t *parent)
{
    unsigned int br, success;
    bool xml_parsed = false;
    struct xml_document *title_xml = NULL;
    char xml_path[DASH_MAX_PATHLEN];

    success = 0;
    strncpy(title->title_folder, title_folder, sizeof(title->title_folder) - 1);

    title->has_xml = true;
    title->has_thumbnail = true;
    if (title_parse(title) == 0)
    {
        nano_debug(LEVEL_ERROR, "ERROR: Could not parse %s\n.", title->title_folder);
        goto title_invalid;
    }

    lv_snprintf(xml_path, sizeof(xml_path), "%s%c_resources%cdefault.xml",
                title_folder, DASH_PATH_SEPARATOR, DASH_PATH_SEPARATOR);
    uint8_t *xml_raw = lv_fs_orc(xml_path, &br);
    if (xml_raw == NULL)
    {
        goto title_no_xml;
    }
    title_xml = xml_parse_document(xml_raw, strlen((char *)xml_raw));
    if (title_xml == NULL)
    {
        nano_debug(LEVEL_ERROR, "ERROR: Could not parse %s\n", xml_path);
        goto title_no_xml;
    }
    struct xml_string *clean_title_string = title_get_synopsis(title_xml, "title");
    if (clean_title_string == NULL)
    {
        nano_debug(LEVEL_ERROR, "ERROR: %s missing \"title\" node\n", xml_path);
        goto title_no_xml;
    }
    xml_string_copy(clean_title_string, (uint8_t *)title->title, xml_string_length(clean_title_string));
    title->title[xml_string_length(clean_title_string)] = '\0';
    xml_parsed = true;

title_no_xml:
    // If no xml was found, fallback to inbuilt title.
    if (xml_parsed == false)
    {
        nano_debug(LEVEL_TRACE, "TRACE: Could not find a valid synopsis xml in %s\n", xml_path);
        for (int i = 0; i < (int)LV_MIN(sizeof(title->title), sizeof(title->xbe_cert.wszTitleName) / 2); i++)
        {
            uint16_t unicode = title->xbe_cert.wszTitleName[i];
            title->title[i] = (unicode > 0x7E) ? '?' : (unicode & 0x7F);
        }
        title->title[sizeof(title->xbe_cert.wszTitleName) / 2] = '\0';
        title->has_xml = false;
        title->has_thumbnail = false;
    }

    // Create an image container
    title->image_container = lv_obj_create(parent);
    title->image_container->user_data = title;

    lv_group_add_obj(lv_group_get_default(), title->image_container);
    title->thumb_jpeg = NULL;
    title->jpeg_handle = NULL;

    // Each image container holds info for the built in default thumbnail
    // which is shown if no jpg exists or its being decompressed still.
    title->thumb_default = lv_img_create(title->image_container);
    lv_img_set_src(title->thumb_default, &default_tbn);
    lv_obj_update_layout(title->thumb_default);
    LV_ASSERT(lv_obj_get_height(title->thumb_default) != 0);
    LV_ASSERT(lv_obj_get_width(title->thumb_default) != 0);
    lv_img_set_pivot(title->thumb_default, 0, 0);
    lv_img_set_zoom(title->thumb_default, DASH_THUMBNAIL_WIDTH * 256 / lv_obj_get_width(title->thumb_default));

    // Setup the image container for the actual jpg thumbnail. It will be stored here if it exists
    lv_obj_add_style(title->image_container, &titleview_image_container_style, LV_PART_MAIN);
    lv_obj_clear_flag(title->image_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(title->image_container, DASH_THUMBNAIL_HEIGHT);
    lv_obj_set_width(title->image_container, DASH_THUMBNAIL_WIDTH);
    lv_obj_update_layout(title->thumb_default);

    // Put some text on the default thumbnail with title
    // if (title->has_thumbnail == false) //We'll just put text on everything for now
    {
        lv_obj_t *label = lv_label_create(title->thumb_default);
        lv_obj_add_style(label, &titleview_image_text_style, LV_PART_MAIN);
        lv_obj_set_width(label, DASH_THUMBNAIL_WIDTH);
        lv_obj_update_layout(label);
        lv_label_set_text_static(label, title->title);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_add_event_cb(title->image_container, jpg_on_screen_cb, LV_EVENT_DRAW_MAIN_BEGIN, title);

    if (title_xml != NULL)
    {
        xml_document_free(title_xml, false);
    }

    if (xml_raw != NULL)
    {
        lv_mem_free(xml_raw);
    }

    if (list_head == NULL)
    {
        list_head = title;
        list_tail = title;
        list_tail->next = NULL;
    }
    else
    {
        list_tail->next = title;
        list_tail = title;
        list_tail->next = NULL;
    }
    success = 1;
title_invalid:
    return success;
}

void titlelist_remove(title_t *title)
{
    jpg_clean(title);
    if (lv_obj_is_valid(title->image_container))
    {
        lv_obj_del(title->image_container);
    }

    // Handle the case if its the head item.
    if (title == list_head)
    {
        list_head = title->next;
        lv_memset(title, 0, sizeof(title_t));
        return;
    }

    // Find it in the linked list
    title_t *_title = list_head;
    while (_title && _title->next != title)
    {
        _title = _title->next;
    }
    if (_title == NULL)
    {
        return;
    }
    _title->next = title->next;
    lv_memset(title, 0, sizeof(title_t));
}
