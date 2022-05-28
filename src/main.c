// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "helpers/nano_debug.h"
#include "dash.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>

// I use SDL for portable thread, mutex and atomic
#include <SDL.h>
static SDL_mutex *lvgl_mutex;

/* clang-format off */
gamecontroller_map_t lvgl_gamecontroller_map[] =
{
    {.sdl_map = SDL_CONTROLLER_BUTTON_A, .lvgl_map = LV_KEY_ENTER},
    {.sdl_map = SDL_CONTROLLER_BUTTON_B, .lvgl_map = LV_KEY_ESC},
    {.sdl_map = SDL_CONTROLLER_BUTTON_X, .lvgl_map = LV_KEY_BACKSPACE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_Y, .lvgl_map = DASH_INFO_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_BACK, .lvgl_map = DASH_INFO_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_GUIDE, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_START, .lvgl_map = DASH_SETTINGS_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSTICK, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSTICK, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, .lvgl_map = DASH_PREV_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, .lvgl_map = DASH_NEXT_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_UP, .lvgl_map = LV_KEY_UP},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_DOWN, .lvgl_map = LV_KEY_DOWN},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_LEFT, .lvgl_map = LV_KEY_LEFT},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_RIGHT, .lvgl_map = LV_KEY_RIGHT}
};
/* clang-format on */

// LVGL isn't thread safe but we can use threads if we put locks around lv_task_handler
// and then any other lv_.. calls from other threads
void lvgl_getlock(void)
{
    SDL_LockMutex(lvgl_mutex);
}

void lvgl_removelock(void)
{
    SDL_UnlockMutex(lvgl_mutex);
}

int main(int argc, char *argv[])
{
    int w,h;
    platform_init(&w, &h);
    platform_network_init();

    lvgl_mutex = SDL_CreateMutex();

    lv_init();
    lv_log_register_print_cb(lvgl_putstring);
    lv_port_disp_init(w, h);
    lv_port_indev_init(false);
    lv_fs_stdio_init();
    lvgl_getlock();
    dash_init();
    lvgl_removelock();

    uint32_t tick_start, tick_elapsed;
    while (lv_get_quit() == LV_QUIT_NONE)
    {
        lvgl_getlock();
        tick_start = lv_tick_get();
        lv_task_handler();
        tick_elapsed = lv_tick_elaps(tick_start);
        lvgl_removelock();
        if (tick_elapsed < LV_DISP_DEF_REFR_PERIOD)
        {
            SDL_Delay(LV_DISP_DEF_REFR_PERIOD - tick_elapsed);
        }
    }
    dash_deinit();
    lv_port_disp_deinit();
    lv_port_indev_deinit();
    lv_deinit();
    platform_quit(lv_get_quit());
    return 0;
}