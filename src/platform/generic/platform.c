// SPDX-License-Identifier: CC0-1.0
// SPDX-FileCopyrightText: 2022 Ryzee119

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "helpers/nano_debug.h"
#include "platform/platform.h"

#include "lvgl.h"
#include "lv_port_indev.h"
#include "dash.h"
#include "dash_styles.h"

void platform_init(int *w, int *h)
{
    *w = 1280;
    *h = 720;
}

const char *platform_show_info_cb(void)
{
    return "No platform info";
}

const char *platform_realtime_info_cb(void)
{
    return "No real-time info";
}

void platform_quit(lv_quit_event_t event)
{
    if (event == LV_REBOOT)
    {
        printf("Reboot event\n");
    }
    else if (event == LV_SHUTDOWN)
    {
        printf("Shutdown event\n");
    }
    else if (event == LV_QUIT_OTHER)
    {
        printf("Other quit event\n");
        const char* launch = dash_get_launch_exe();
    }
}
