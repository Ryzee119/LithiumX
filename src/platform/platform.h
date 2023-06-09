// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _PLATFORM_H
#define _PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

/*
 * Anything you need to do to initialise your platform
*/
void platform_init(int *w, int *h);

/*
 * Anything you need to do when exiting lithiumX
*/
void platform_quit(lv_quit_event_t event);

/*
 * Passes a container that you can add whatever system specific
 * info you want.
*/
void platform_system_info(lv_obj_t *window);

/*
 * Xbox Specific. Flush cache partitions
*/
void platform_flush_cache(void);

/*
 * Retrieve the current date and time in iso 8601 format.
 * YYYY-MM-DD HH:MM:SS
 */
void platform_get_iso8601_time(char time_str[20]);

#ifdef __cplusplus
}
#endif

#endif