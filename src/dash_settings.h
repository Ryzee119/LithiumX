// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_SETTINGS_H
#define _DASH_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

#define DASH_SETTINGS_VERSION 0x01
#define DASH_SETTINGS_MAGIC (0xBEEF0000+DASH_SETTINGS_VERSION)
typedef struct dash_settings {
    unsigned int magic;
    bool use_fahrenheit;
    bool auto_launch_dvd;
    bool show_debug_info;
    int startup_page_index;
    int theme_colour;
    int max_recent_items;
    char earliest_recent_date[20]; //"YYYY-MM-DD HH:MM:SS"
    char sort_strings[4096]; //Like "Games=1 Apps=1\0" etc.
} dash_settings_t;

void dash_settings_open(void);
void dash_settings_apply(bool confirm_box);
void dash_settings_read(void);

#ifdef __cplusplus
}
#endif

#endif
