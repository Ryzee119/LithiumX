// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lithiumx.h"

#ifdef NXDK
#include <xboxkrnl/xboxkrnl.h>
#include <hal/video.h>
#define VIDEO_REGION_PAL 0x00000300
#else
//VIDEO FLAGS
#define VIDEO_MODE_720P  0x020000
#define VIDEO_MODE_1080I 0x040000
#define VIDEO_MODE_480P  0x080000
#define VIDEO_WIDESCREEN 0x010000
#define VIDEO_LETTERBOX  0x100000
#define VIDEO_60Hz       0x400000
#define VIDEO_50Hz       0x800000
#define VIDEO_REGION_PAL 0x000300
#endif

//Audio flags
#define AUDIO_MODE_STEREO 0x00000
#define AUDIO_MODE_MONO 0x00001
#define AUDIO_MODE_SURROUND 0x00002
#define AUDIO_MODE_AC3 0x10000
#define AUDIO_MODE_DTS 0x20000

#ifdef NXDK
static DWORD video_flags;
static DWORD audio_flags;

/* AUDIO SETTINGS */
static void audio_apply_settings()
{
    ExSaveNonVolatileSetting(XC_AUDIO, 4, &audio_flags, sizeof(audio_flags));
}

static void audio_mode_enable(void *param)
{
    DWORD flag_changed = (DWORD)param;

    // Msdash, automatically sets audio mode to surround if AC3 enabled
    if (flag_changed == AUDIO_MODE_AC3)
    {
        audio_flags &= ~0x03;
        audio_flags |= AUDIO_MODE_SURROUND;
    }

    audio_flags |= flag_changed;

    audio_apply_settings();
}

static void audio_mode_disable(void *param)
{
    DWORD flag_changed = (DWORD)param;
    audio_flags &= ~flag_changed;
    audio_apply_settings();
}

static void audio_speaker_config_changed(void *param)
{
    DWORD flag_changed = (DWORD)param;

    audio_flags &= ~0x03;
    audio_flags |= flag_changed;

    // Msdash disables AC3 is you select mono or stereo.
    if (flag_changed == AUDIO_MODE_MONO || flag_changed == AUDIO_MODE_STEREO)
    {
        audio_flags &= ~AUDIO_MODE_AC3;    
    }
    audio_apply_settings();
}

static void audio_speaker_config_change(void *param)
{
    static const menu_items_t items[] =
        {
            {"Mono", audio_speaker_config_changed, (void *)(AUDIO_MODE_MONO), NULL},
            {"Stereo", audio_speaker_config_changed, (void *)(AUDIO_MODE_STEREO), NULL},
            {"Dolby Surround", audio_speaker_config_changed, (void *)(AUDIO_MODE_SURROUND), NULL}
        };

    lv_obj_t *menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    // Highlight the active setting
    if (audio_flags & AUDIO_MODE_MONO)
        menu_force_value(menu, 0);
    else if (audio_flags & AUDIO_MODE_SURROUND)
        menu_force_value(menu, 2);
    else
        menu_force_value(menu, 1);
}

static void audio_mode_change(void *param)
{
    menu_items_t items[] =
    {
        {"Enable", audio_mode_enable, param, NULL},
        {"Disable", audio_mode_disable, param, NULL},
    };
    lv_obj_t *menu = menu_open(items, DASH_ARRAY_SIZE(items));
    menu_force_value(menu, (audio_flags & (DWORD)param) ? 0 : 1);
}

static void dash_eeprom_audio_settings_open()
{
    static const menu_items_t items[] =
        {
            {"Speaker Config", audio_speaker_config_change, NULL, NULL},
            {"Dolby Digital", audio_mode_change, (void *)(AUDIO_MODE_AC3), NULL},
            {"DTS", audio_mode_change, (void *)(AUDIO_MODE_DTS), NULL}};

    lv_obj_t *menu;
    DWORD v_adaptor = XVideoGetEncoderSettings() & VIDEO_ADAPTER_MASK;
    if (v_adaptor == AV_PACK_STANDARD)
    {
        // Drop DTS and Dolby Digital Options with composite
        menu = menu_open_static(items, DASH_ARRAY_SIZE(items) - 2);
    }
    else
    {
        menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    }
}

/* VIDEO SETTINGS */
static void video_apply_settings()
{
    ExSaveNonVolatileSetting(XC_VIDEO, 4, &video_flags, sizeof(video_flags));
}

static void video_ratio_changed(void *param)
{
    DWORD flag_changed = (DWORD)param;
    switch (flag_changed)
    {
    case (VIDEO_WIDESCREEN | VIDEO_LETTERBOX):
        video_flags &= ~VIDEO_WIDESCREEN;
        video_flags &= ~VIDEO_LETTERBOX;
        break;
    case VIDEO_LETTERBOX:
        video_flags &= ~VIDEO_WIDESCREEN;
        video_flags |= VIDEO_LETTERBOX;
        break;
    case VIDEO_WIDESCREEN:
        video_flags |= VIDEO_WIDESCREEN;
        video_flags &= ~VIDEO_LETTERBOX;
        break;
    default:
        return;
    }
    video_apply_settings();
}

static void video_ratio_change(void *param)
{
    static const menu_items_t items[] =
    {
        {"Normal", video_ratio_changed, (void *)(VIDEO_WIDESCREEN | VIDEO_LETTERBOX), NULL},
        {"Widescreen", video_ratio_changed, (void *)VIDEO_WIDESCREEN, NULL},
        {"Letterbox", video_ratio_changed, (void *)VIDEO_LETTERBOX, NULL},
    };
    lv_obj_t *menu = menu_open_static(items, DASH_ARRAY_SIZE(items));
    if (video_flags & VIDEO_LETTERBOX)
    {
        menu_force_value(menu, 2);
    }
    else if (video_flags & VIDEO_WIDESCREEN)
    {
        menu_force_value(menu, 1);
    }
}

static void video_mode_enable(void *param)
{
    video_flags |= (DWORD)param;
    video_apply_settings();
}

static void video_mode_disable(void *param)
{
    video_flags &= ~((DWORD)param);
    video_apply_settings();
}

static void video_mode_change(void *param)
{
    menu_items_t items[] =
    {
        {"Enable", video_mode_enable, param, NULL},
        {"Disable", video_mode_disable, param, NULL},
    };
    lv_obj_t *menu = menu_open(items, DASH_ARRAY_SIZE(items));
    menu_force_value(menu, (video_flags & (DWORD)param) ? 0 : 1);
}

static void dash_eeprom_video_settings_open()
{
    int num_items;
    ULONG type;
    DWORD av_region, v_adaptor;
    ExQueryNonVolatileSetting(XC_FACTORY_AV_REGION, &type, &av_region, sizeof(av_region), NULL);

    av_region &= VIDEO_STANDARD_MASK;
    v_adaptor = XVideoGetEncoderSettings() & VIDEO_ADAPTER_MASK;

    static const menu_items_t items_hd_ntsc[] =
    {
        {"480p", video_mode_change, (void *)VIDEO_MODE_480P, NULL},
        {"720p", video_mode_change, (void *)VIDEO_MODE_720P, NULL},
        {"1080i", video_mode_change, (void *)VIDEO_MODE_1080I, NULL},
    };

    static const menu_items_t items_pal[] =
    {
        {"PAL60", video_mode_change, (void *)VIDEO_60Hz, NULL},
    };

    static const menu_items_t items_common[] =
    {
        {"Change Aspect Ratio", video_ratio_change, NULL, NULL},
    };

    menu_items_t *items = lv_mem_alloc(sizeof(menu_items_t) * 4);
    lv_memcpy(items, items_common, sizeof(items_common));
    num_items = DASH_ARRAY_SIZE(items_common);

    // On PAL systems PAL60 options is visible
    if (av_region == VIDEO_REGION_PAL)
    {
        lv_memcpy(&items[1], items_pal, sizeof(items_pal));
        num_items += DASH_ARRAY_SIZE(items_pal);
    }
    // On NTSC systems, with HD pack, HD options are selected 
    else if (v_adaptor == AV_PACK_HDTV)
    {
        lv_memcpy(&items[1], items_hd_ntsc, sizeof(items_hd_ntsc));
        num_items += DASH_ARRAY_SIZE(items_hd_ntsc);
    }

    lv_obj_t *menu = menu_open_static(items, num_items);
}

void dash_eeprom_settings_open()
{
    static const menu_items_t items[] =
        {
            {"Audio " LV_SYMBOL_AUDIO, dash_eeprom_audio_settings_open, NULL, NULL},
            {"Video " LV_SYMBOL_VIDEO, dash_eeprom_video_settings_open, NULL, NULL},
            //{"Clock " LV_SYMBOL_BELL, dash_eeprom_clock_settings_open, NULL, NULL},
        };

    ULONG type;
    ExQueryNonVolatileSetting(XC_VIDEO, &type, &video_flags, sizeof(video_flags), NULL);
    ExQueryNonVolatileSetting(XC_AUDIO, &type, &audio_flags, sizeof(audio_flags), NULL);

    DWORD v_adaptor = XVideoGetEncoderSettings() & VIDEO_ADAPTER_MASK;
    // RF drop audio settings entirely. This is what MSDash Does
    if (v_adaptor == AV_PACK_RFU)
    {
        menu_open_static(&items[1], DASH_ARRAY_SIZE(items) - 1);
    }
    else
    {
        menu_open_static(items, DASH_ARRAY_SIZE(items));
    }
}

#else
void dash_eeprom_settings_open()
{
    
}
#endif