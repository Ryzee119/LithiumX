// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lvgl/src/misc/lv_lru.h"
#include "dash.h"
#include "dash_styles.h"
#include "dash_synop.h"
#include "dash_titlelist.h"
#include "xml/src/xml.h"
#include "helpers/fileio.h"
#include "helpers/nano_debug.h"
#include <stdlib.h>
#include <stdio.h>

#define SYNOP_WIDTH 600
#define SYNOP_HEIGHT 440
#define SYNOP_CACHE_SIZE 6

typedef struct
{
    title_t *title;
    lv_obj_t *synop_menu;
} synop_cache_t;
lv_lru_t *synop_cache;
static void cache_free(synop_cache_t *synop)
{
    lv_obj_del(synop->synop_menu);
    lv_mem_free(synop);
}

static void close_callback(lv_event_t *event)
{
    lv_obj_t *synop_menu = (lv_obj_t *)event->user_data;
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_ESC || key == LV_KEY_ENTER || key == DASH_INFO_PAGE)
    {
        lv_group_t *gp = lv_group_get_default();
        lv_group_focus_freeze(gp, false);
        lv_group_focus_obj((lv_obj_t *)synop_menu->user_data);
        // We only hide it and keep is cached for now
        lv_obj_add_flag(synop_menu, LV_OBJ_FLAG_HIDDEN);
    }
}

// Helper function to apply a consistent style to the text
static void apply_label_style(lv_obj_t *label)
{
    lv_label_set_recolor(label, true);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_style_value_t value;
    lv_style_get_prop(&synop_container_style, LV_STYLE_PAD_LEFT, &value);
    lv_obj_set_width(label, lv_obj_get_width(lv_obj_get_parent(label)) - 2 * value.num);
    lv_obj_add_style(label, &synop_text_style, 0);
    lv_obj_update_layout(label);
}

// Helper function to extract and xml string, and create a new label with the formatted text
static lv_obj_t *new_label(lv_obj_t *parent, struct xml_string *t, const char *prefix)
{
    lv_obj_t *label;
    size_t slen = xml_string_length(t);
    char *str;

    label = lv_label_create(parent);

    str = lv_mem_alloc(slen + 1);
    xml_string_copy(t, (uint8_t *)str, slen);
    str[slen] = '\0';

    lv_label_set_text_fmt(label, "%s %s# %s", DASH_MENU_COLOR, prefix, str);
    lv_mem_free(str);
    apply_label_style(label);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    return label;
}

// Basic containers dont scroll. We registered a custom scroll callback. This is handled here.
static void synop_scroll(lv_event_t *e)
{
    // objects normally scroll automatically however core object containers dont have any animations.
    // I replace the scrolling with my own here with animation enabled.
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    if (key == LV_KEY_UP)
    {
        lv_obj_scroll_to_y(obj, lv_obj_get_scroll_y(obj) - lv_obj_get_height(obj) / 4, LV_ANIM_ON);
    }
    if (key == LV_KEY_DOWN)
    {
        lv_obj_scroll_to_y(obj, lv_obj_get_scroll_y(obj) + lv_obj_get_height(obj) / 4, LV_ANIM_ON);
    }
}

void synop_menu_init(void)
{
    synop_cache = lv_lru_create(sizeof(synop_cache_t) * SYNOP_CACHE_SIZE,
                                sizeof(synop_cache_t), (lv_lru_free_t *)cache_free, NULL);
}

void synop_menu_deinit(void)
{
    lv_lru_del(synop_cache);
}

// Opens the synopsis menu for the given title
void synop_menu_open(title_t *title)
{
    synop_cache_t *synop = NULL;
    char xml_path[DASH_MAX_PATHLEN];
    bool has_synop;
    lv_obj_t *label;
    lv_obj_t *synop_menu;
    struct xml_document *xml_handle;
    struct xml_string *t;
    uint32_t br;

    nano_debug(LEVEL_TRACE, "TRACE: Opening synopsis menu for %s\n", title->title_str);

    lv_group_t *gp = lv_group_get_default();

    // Check if synop is cached
    lv_lru_get(synop_cache, &title, sizeof(title), (void **)&synop);
    if (synop != NULL)
    {
        synop_menu = synop->synop_menu;
        lv_obj_clear_flag(synop_menu, LV_OBJ_FLAG_HIDDEN);
        nano_debug(LEVEL_TRACE, "TRACE: Found synopsis menu in cache\n");
        goto cache_leave;
    }

    // Synop isnt cached. We need to create it
    xml_handle = NULL;
    has_synop = false;

    // Create a basic container, make it vertically scrollable, fixed size, center aligned
    // and apply flex layout so we can add text easily.
    synop_menu = lv_obj_create(lv_scr_act());
    lv_obj_add_style(synop_menu, &synop_container_style, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(synop_menu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(synop_menu, LV_OBJ_FLAG_SCROLLABLE);             // Use my own scroll callback
    lv_obj_add_event_cb(synop_menu, synop_scroll, LV_EVENT_KEY, NULL); // Setup scroll callback
    lv_obj_set_scroll_dir(synop_menu, LV_DIR_TOP);                     // Only scroll up and down
    lv_obj_set_size(synop_menu, SYNOP_WIDTH, SYNOP_HEIGHT);
    lv_obj_align(synop_menu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(synop_menu, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(synop_menu, LV_FLEX_FLOW_COLUMN); // When each label is added, it is added below the previous
    lv_obj_update_layout(synop_menu);

    // Add a title to the synopsis box
    label = lv_label_create(synop_menu);
    lv_label_set_text_fmt(label, "%s Title:# %s", DASH_MENU_COLOR, title->title_str);
    apply_label_style(label);

    // If the title doesnt have an xml. We are done here.
    if (title->has_xml == false)
    {
        nano_debug(LEVEL_WARN, "WARN: %s does not have a synopsis menu.\n", title->title_str);
        goto leave;
    }

    lv_snprintf(xml_path, sizeof(xml_path), "%s%c_resources%cdefault.xml", title->title_folder, DASH_PATH_SEPARATOR, DASH_PATH_SEPARATOR);

    uint8_t *xml_raw = lv_fs_orc(xml_path, &br);
    if (xml_raw == NULL)
    {
        // We shouldnt get here if the file doesnt exist or is invalid so we log an error
        nano_debug(LEVEL_ERROR, "ERROR: Could not open %s\n", xml_path);
        goto leave;
    }

    xml_handle = xml_parse_document(xml_raw, strlen((char *)xml_raw));
    if (xml_handle == NULL)
    {
        lv_mem_free(xml_raw);
        goto leave;
    }

    nano_debug(LEVEL_TRACE, "TRACE: Synopsis for %s seems ok. Parsing it\n", title->title_str);
    has_synop = true;

    t = title_get_synopsis(xml_handle, "developer");
    if (t != NULL)
    {
        label = new_label(synop_menu, t, "Developer: ");
    }

    t = title_get_synopsis(xml_handle, "publisher");
    if (t != NULL)
    {
        label = new_label(synop_menu, t, "Publisher: ");
    }

    t = title_get_synopsis(xml_handle, "release_date");
    if (t != NULL)
    {
        label = new_label(synop_menu, t, "Release Date: ");
    }

    t = title_get_synopsis(xml_handle, "rating");
    if (t != NULL)
    {
        label = new_label(synop_menu, t, "Rating: ");
        lv_label_ins_text(label, LV_LABEL_POS_LAST, "/10");
    }

    t = title_get_synopsis(xml_handle, "overview");
    if (t != NULL)
    {
        label = new_label(synop_menu, t, "Overview:");
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_update_layout(label);
    }

    xml_document_free(xml_handle, false);
    lv_mem_free(xml_raw);
leave:;
    // Add it to Synopsis cache
    synop = lv_mem_alloc(sizeof(synop_cache_t));
    synop->title = title;
    synop->synop_menu = synop_menu;
    lv_lru_set(synop_cache, &title, sizeof(title), synop, sizeof(synop_cache_t));

    if (has_synop == false)
    {
        label = lv_label_create(synop_menu);
        lv_label_set_text(label, "No synopsis found for this title");
        apply_label_style(label);
    }
    // We need to add the obj to the input group so we register button events.
    // Freeze focus on the synop box as we dont want lvgl change focus for us
    // Remember the previously focused object so we jump back to it when we close the synopsis menu
    lv_group_add_obj(gp, synop_menu);
    lv_obj_add_event_cb(synop_menu, close_callback, LV_EVENT_KEY, synop_menu);
cache_leave:;
    lv_obj_t *old_focus = lv_group_get_focused(gp);
    lv_group_focus_obj(synop_menu);
    lv_group_focus_freeze(gp, true);
    synop_menu->user_data = old_focus;
}
