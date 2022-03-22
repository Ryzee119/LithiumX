// SPDX-License-Identifier: MIT

#include <assert.h>
#include <SDL.h>
#include "src/draw/sdl/lv_draw_sdl.h"
#include "lv_port_disp.h"
#include "lvgl.h"

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static int DISPLAY_WIDTH;
static int DISPLAY_HEIGHT;

#ifndef WINDOW_NAME
#define WINDOW_NAME "LVGL"
#endif

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    SDL_RenderPresent(renderer);
    SDL_RenderClear(renderer);
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(int width, int height)
{
    assert(LV_COLOR_DEPTH == 16 || LV_COLOR_DEPTH == 32);
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(WINDOW_NAME,
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);

#ifdef SDL_USE_SW_RENDERER
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#else
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif

    lv_disp_draw_buf_init(&draw_buf, NULL, NULL, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    disp_drv.user_data = renderer;
    lv_disp_drv_register(&disp_drv);

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_RenderClear(renderer);
}

void lv_port_disp_deinit()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
