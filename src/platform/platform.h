// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _PLATFORM_H
#define _PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "BUILD_VERSION not set"
#endif
void platform_init(int *w, int *h);
void platform_flush_cache_cb(); //Xbox only
void platform_launch_dvd(); //Xbox only
const char *platform_realtime_info_cb(void); //Xbox only
void platform_show_info_cb(lv_obj_t *parent);
void platform_quit(lv_quit_event_t event);

#ifdef __cplusplus
}
#endif

#endif
