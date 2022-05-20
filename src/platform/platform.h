// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _PLATFORM_H
#define _PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "lv_port_indev.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "BUILD_VERSION not set"
#endif
void platform_init(int *w, int *h);
void platform_flush_cache_cb(); //Xbox only
void platform_launch_dvd(); //Xbox only
const char *platform_realtime_info_cb(void); //Xbox only
const char *platform_show_info_cb(void);
void platform_quit(lv_quit_event_t event);

void platform_network_init(void);
void platform_networkrestart(void);
void platform_networkdeinit(void);
int platform_networkget_up(void);
uint32_t platform_network_get_ip(char *rxbuf, uint32_t max_len);
uint32_t platform_network_get_gateway(char *rxbuf, uint32_t max_len);
uint32_t platform_network_get_netmask(char *rxbuf, uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif
