// SPDX-License-Identifier: MIT

//#ifdef NXDK
#if 1
#include <assert.h>
#include <hal/video.h>
#include "lv_port_disp.h"
#include "lvgl.h"
#include "pbgl/lv_draw_pbgl.h"

#include <pbgl.h>
#include <GL/gl.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static int DISPLAY_WIDTH;
static int DISPLAY_HEIGHT;

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    pbgl_swap_buffers();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(int width, int height)
{
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;
    XVideoSetMode(DISPLAY_WIDTH, DISPLAY_HEIGHT, LV_COLOR_DEPTH, REFRESH_DEFAULT);

    lv_disp_draw_buf_init(&draw_buf, NULL, NULL, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_ctx_init = lv_draw_gl_init_ctx;
    disp_drv.draw_ctx_deinit = lv_draw_gl_deinit_ctx;
    disp_drv.draw_ctx_size = sizeof(lv_draw_gl_ctx_t);

    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    pbgl_init(GL_TRUE);
    pbgl_set_swap_interval(1);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    float ar = (float)DISPLAY_WIDTH / (float)DISPLAY_HEIGHT;
    glViewport(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, -10, 10);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void lv_port_disp_deinit()
{

}

#endif
