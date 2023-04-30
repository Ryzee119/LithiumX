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
#include <xboxkrnl/xboxkrnl.h>
#include <hal/video.h>
#else
#define VIDEO_MODE_720P 0x20000
#define VIDEO_MODE_1080I 0x40000
#define VIDEO_MODE_480P 0x80000
#define VIDEO_WIDESCREEN 0x010000
#define VIDEO_LETTERBOX 0x100000
#endif

#define AUDIO_MODE_STEREO 0x00000
#define AUDIO_MODE_MONO 0x00001
#define AUDIO_MODE_SURROUND 0x00002
#define AUDIO_MODE_ENABLE_AC3 0x10000
#define AUDIO_MODE_ENABLE_DTS 0x20000

#define COLOR_DESELECTED lv_color_make(15, 15, 15)
#define COLOR_SELECTED lv_color_make(61, 153, 0)

static lv_obj_t *eeprom_container;
static lv_obj_t *switch_480p, *switch_720p, *switch_1080i, *switch_ac3, *switch_dts;
static lv_obj_t *roller_video, *roller_audio;
lv_color_t default_color;
static uint32_t video_settings;
static uint32_t audio_settings;

static void eeprom_update(void *param)
{
    lv_state_t state;
    lv_obj_t *objs[] = {switch_480p, switch_720p, switch_1080i, switch_ac3, switch_dts};
    uint32_t setting[] = {video_settings, video_settings, video_settings, audio_settings, audio_settings};
    uint32_t mask[] = {VIDEO_MODE_480P, VIDEO_MODE_720P, VIDEO_MODE_1080I, AUDIO_MODE_ENABLE_AC3, AUDIO_MODE_ENABLE_DTS};

#ifdef NXDK
    ULONG type;
    ExQueryNonVolatileSetting(XC_VIDEO, &type, &video_settings, sizeof(video_settings), NULL);
    ExQueryNonVolatileSetting(XC_AUDIO, &type, &audio_settings, sizeof(audio_settings), NULL);
#endif

    for (int i = 0; i < sizeof(objs) / sizeof(lv_obj_t *); i++)
    {
        state = lv_obj_get_state(objs[i]);
        lv_obj_clear_state(objs[i], LV_STATE_CHECKED);
        lv_obj_add_state(objs[i], (setting[i] & mask[i]) ? LV_STATE_CHECKED : 0);
        if (state != lv_obj_get_state(objs[i]))
        {
            lv_event_send(objs[i], LV_EVENT_VALUE_CHANGED, NULL);
            state = lv_obj_get_state(objs[i]);
        }
        lv_color_t knob_color = (state & LV_STATE_CHECKED) ? lv_color_make(119, 221, 119) : lv_color_make(255, 105, 97);
        lv_obj_set_style_bg_color(objs[i], knob_color, LV_PART_KNOB);
    }

    lv_roller_set_selected(roller_video, (video_settings & VIDEO_WIDESCREEN) ? 2 :
                                         (video_settings & VIDEO_LETTERBOX) ? 1 : lv_roller_get_selected(roller_video), LV_ANIM_ON);

    lv_roller_set_selected(roller_audio, (audio_settings & AUDIO_MODE_MONO) ? 2 :
                                         (audio_settings & AUDIO_MODE_SURROUND) ? 1 : lv_roller_get_selected(roller_audio), LV_ANIM_ON);
}

void eeprom_backup_cb(lv_timer_t *t)
{
    lv_obj_t *label = t->user_data;
    lv_label_set_text(label, "Backup EEPROM");
}

static void eeprom_backup(lv_obj_t *obj)
{
    const char *outcome = "Failed to write to E:\\eeprom.bin";
    lv_obj_t *label = lv_obj_get_child(obj, 0);
    #ifdef NXDK
    uint8_t buffer[0x100];
    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < sizeof(buffer); i += 2)
    {
        ULONG value;
        if (!NT_SUCCESS(HalReadSMBusValue(0xA9, (UCHAR)i, TRUE, (PULONG)&value)))
        {
            fflush(stdout);
        }
        *ptr++ = (uint16_t)value;
    }
    lv_fs_file_t fp;
    uint32_t bw;
    if (lv_fs_open(&fp, "Q:E:\\eeprom.bin", LV_FS_MODE_WR) == LV_FS_RES_OK)
    {
        lv_fs_write(&fp, buffer, sizeof(buffer), &bw);
        lv_fs_close(&fp);
        outcome = "Wrote to E:\\eeprom.bin";
    }
    #endif
    lv_label_set_text(label, outcome);
    lv_timer_set_repeat_count(lv_timer_create(eeprom_backup_cb, 1000, label), 1);
}

static void key_press(lv_event_t *e)
{
    static int child_index = 0;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_key_t key = lv_indev_get_key(lv_indev_get_act());
    lv_obj_t *child = lv_obj_get_child(obj, child_index);
    if (key == LV_KEY_UP || key == LV_KEY_DOWN)
    {
        lv_obj_set_style_bg_color(child, COLOR_DESELECTED, LV_PART_MAIN);
        lv_obj_set_style_bg_color(child, COLOR_DESELECTED, LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(child, COLOR_DESELECTED, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(child, COLOR_DESELECTED, LV_PART_SELECTED);

        child_index += (key == LV_KEY_UP) ? -1 : (key == LV_KEY_DOWN) ? 1
                                                                      : 0;
        child_index = LV_MAX(0, child_index);
        child_index = LV_MIN(lv_obj_get_child_cnt(obj) - 1, child_index);
        child = lv_obj_get_child(obj, child_index);

        lv_obj_set_style_bg_color(child, COLOR_SELECTED, LV_PART_MAIN);
        lv_obj_set_style_bg_color(child, COLOR_SELECTED, LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(child, COLOR_SELECTED, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(child, COLOR_SELECTED, LV_PART_SELECTED);
    }
    else if (key == LV_KEY_ENTER)
    {
        if (lv_obj_check_type(child, &lv_switch_class))
        {
            lv_state_t state = lv_obj_get_state(child);
            lv_obj_clear_state(child, LV_STATE_DISABLED);
            lv_obj_clear_state(child, LV_STATE_CHECKED);
            lv_obj_add_state(child, (state & LV_STATE_CHECKED) ? LV_STATE_DISABLED : LV_STATE_CHECKED);
            lv_event_send(child, LV_EVENT_VALUE_CHANGED, NULL);
        }
        else if (lv_obj_check_type(child, &lv_roller_class))
        {
            int selected = lv_roller_get_selected(child) + 1;
            // Infinte scroll hack
            if (selected == lv_roller_get_option_cnt(child))
            {
                lv_roller_set_selected(child, 0, LV_ANIM_OFF);
                selected = 1;
                lv_obj_update_layout(child);
            }
            lv_roller_set_selected(child, selected, LV_ANIM_ON);
            lv_event_send(child, LV_EVENT_VALUE_CHANGED, NULL);
        }
        else if (lv_obj_check_type(child, &lv_btn_class))
        {
            eeprom_backup(child);
        }
        else
        {
        }
    }
    else if (key == LV_KEY_ESC)
    {
        menu_hide_item(eeprom_container);
    }
}

static void switch_video_handler(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t mask = (uint32_t)lv_event_get_user_data(e);
    if (lv_obj_has_state(obj, LV_STATE_CHECKED))
    {
        video_settings |= mask;
    }
    else
    {
        video_settings &= ~mask;
    }
#ifdef NXDK
    ExSaveNonVolatileSetting(XC_VIDEO, 4, &video_settings, sizeof(video_settings));
    lv_async_call(eeprom_update, NULL);
#endif
}

static void switch_audio_handler(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t mask = (uint32_t)lv_event_get_user_data(e);
    if (lv_obj_has_state(obj, LV_STATE_CHECKED))
    {
        audio_settings |= mask;
        if (mask == AUDIO_MODE_ENABLE_AC3)
        {
            // Msdash, automatically sets audio mode to surround if AC3 enabled.
            audio_settings &= ~0b11;
            audio_settings |= AUDIO_MODE_SURROUND;
        }
    }
    else
    {
        audio_settings &= ~mask;
    }
#ifdef NXDK
    ExSaveNonVolatileSetting(XC_AUDIO, 4, &audio_settings, sizeof(audio_settings));
    lv_async_call(eeprom_update, NULL);
#endif
}

static void roller_audio_handler(lv_event_t *e)
{
    char buf[32];
    lv_obj_t *obj = lv_event_get_target(e);
    lv_roller_get_selected_str(obj, buf, sizeof(buf));
    if (strcmp(buf, "Mono") == 0)
    {
        audio_settings &= ~0b11;
        audio_settings |= AUDIO_MODE_MONO;
        // Msdash disables AC3 is you select mono or stereo.
        audio_settings &= ~AUDIO_MODE_ENABLE_AC3;
    }
    else if (strcmp(buf, "Stereo") == 0)
    {
        audio_settings &= ~0b11;
        audio_settings |= AUDIO_MODE_STEREO;
        // Msdash disables AC3 is you select mono or stereo.
        audio_settings &= ~AUDIO_MODE_ENABLE_AC3;
    }
    else if (strcmp(buf, "Dolby Surround") == 0)
    {
        audio_settings &= ~0b11;
        audio_settings |= AUDIO_MODE_SURROUND;
    }
#ifdef NXDK
    ExSaveNonVolatileSetting(XC_AUDIO, 4, &audio_settings, sizeof(audio_settings));
    eeprom_update(NULL);
#endif
}

static void roller_video_handler(lv_event_t *e)
{
    // const char * opts_video = "Normal\nLetter Box\nWide Screen\nNormal";
    char buf[32];
    lv_obj_t *obj = lv_event_get_target(e);
    lv_roller_get_selected_str(obj, buf, sizeof(buf));
    if (strcmp(buf, "Normal") == 0)
    {
        video_settings &= ~VIDEO_WIDESCREEN;
        video_settings &= ~VIDEO_LETTERBOX;
    }
    else if (strcmp(buf, "Letter Box") == 0)
    {
        video_settings &= ~VIDEO_WIDESCREEN;
        video_settings |= VIDEO_LETTERBOX;
    }
    else if (strcmp(buf, "Wide Screen") == 0)
    {
        video_settings |= VIDEO_WIDESCREEN;
        video_settings &= ~VIDEO_LETTERBOX;
    }
#ifdef NXDK
    ExSaveNonVolatileSetting(XC_VIDEO, 4, &video_settings, sizeof(video_settings));
    eeprom_update(NULL);
#endif
}

void eeprom_init(void)
{
    lv_obj_t *obj;

    // Setup the container
    eeprom_container = lv_obj_create(lv_scr_act());
    lv_group_add_obj(lv_group_get_default(), eeprom_container);
    menu_apply_style(eeprom_container);
    lv_obj_set_style_pad_row(eeprom_container, 0, LV_PART_MAIN);
    lv_obj_set_size(eeprom_container, MENU_WIDTH*2/3, LV_SIZE_CONTENT);
    eeprom_container->user_data = lv_mem_alloc(sizeof(menu_data_t));
    lv_obj_set_flex_flow(eeprom_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(eeprom_container, key_press, LV_EVENT_KEY, NULL);
    lv_obj_update_layout(eeprom_container);

    lv_obj_t **objs[] = {&switch_480p, &switch_720p, &switch_1080i, &switch_ac3, &switch_dts};
    lv_event_cb_t cbs[] = {switch_video_handler, switch_video_handler, switch_video_handler, switch_audio_handler, switch_audio_handler};
    uint32_t mask[] = {VIDEO_MODE_480P, VIDEO_MODE_720P, VIDEO_MODE_1080I, AUDIO_MODE_ENABLE_AC3, AUDIO_MODE_ENABLE_DTS};
    const char *labels[] = {"480p", "720p", "1080i", "Dolby Digital", "DTS"};

    obj = lv_btn_create(eeprom_container);
    obj = lv_label_create(obj);
    lv_label_set_text(obj, "Backup EEPROM");
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);

    for (int i = 0; i < sizeof(objs) / sizeof(lv_obj_t *); i++)
    {
        obj = lv_switch_create(eeprom_container);
        lv_obj_add_event_cb(obj, cbs[i], LV_EVENT_VALUE_CHANGED, (void *)mask[i]);
        lv_group_remove_obj(obj);
        *objs[i] = obj;
        obj = lv_label_create(obj);
        lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(obj, labels[i]);
    }

    // Video Mode Roller
    const char *opts_video = "Normal\nLetter Box\nWide Screen\nNormal";
    obj = lv_roller_create(eeprom_container);
    lv_obj_add_event_cb(obj, roller_video_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_roller_set_options(obj, opts_video, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(obj, 1);
    lv_group_remove_obj(obj);
    roller_video = obj;

    // Audio Mode roller
    const char *opts_audio = "Stereo\nDolby Surround\nMono\nStereo";
    obj = lv_roller_create(eeprom_container);
    lv_obj_add_event_cb(obj, roller_audio_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_roller_set_options(obj, opts_audio, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(obj, 1);
    lv_group_remove_obj(obj);
    roller_audio = obj;

    // Apply themes
    for (int i = 0; i < lv_obj_get_child_cnt(eeprom_container); i++)
    {
        obj = lv_obj_get_child(eeprom_container, i);
        lv_obj_set_width(obj, lv_obj_get_width(eeprom_container) -
                                  lv_obj_get_style_pad_left(eeprom_container, LV_PART_MAIN) -
                                  lv_obj_get_style_pad_right(eeprom_container, LV_PART_MAIN) -
                                  lv_obj_get_style_border_width(eeprom_container, LV_PART_MAIN) * 2);

        lv_obj_add_style(obj, &eeprom_items_style, LV_PART_MAIN);
        lv_obj_add_style(obj, &eeprom_items_style, LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_add_style(obj, &eeprom_items_style, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &eeprom_items_style, LV_PART_SELECTED);

        lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_KNOB);
        lv_obj_set_style_radius(obj, 0, LV_PART_KNOB);

        lv_obj_set_height(obj, 40);
        lv_obj_update_layout(obj);
    }

    lv_obj_set_style_bg_color(lv_obj_get_child(eeprom_container, 0), COLOR_SELECTED, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_obj_get_child(eeprom_container, 0), COLOR_SELECTED, LV_PART_INDICATOR | LV_STATE_CHECKED);

    // Hidden for now
    lv_obj_add_flag(eeprom_container, LV_OBJ_FLAG_HIDDEN);

    lv_async_call(eeprom_update, NULL);
}

void eeprom_deinit(void)
{
    lv_mem_free(eeprom_container->user_data);
    lv_obj_del(eeprom_container);
    eeprom_container = NULL;
}

void eeprom_open(void)
{
    nano_debug(LEVEL_TRACE, "TRACE: Opening eeprom settings\n");
    menu_show_item(eeprom_container, NULL);
    eeprom_update((NULL));
}
