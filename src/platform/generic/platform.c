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

void platform_show_info_cb(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_recolor(label, true);
    lv_label_set_text_fmt(label, "%s Build Commit:# %s", DASH_MENU_COLOR, BUILD_VERSION);
}

const char *platform_realtime_info_cb(void)
{
    return "No info";
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
    }
}
