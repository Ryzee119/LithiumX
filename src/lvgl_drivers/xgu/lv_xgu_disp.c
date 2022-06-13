// SPDX-License-Identifier: MIT

#include <hal/video.h>
#include <hal/debug.h>
#include <lvgl.h>
#include <xgu.h>
#include <xgux.h>
#include "lv_port_disp.h"
#include "lv_xgu_draw.h"
#include "lvgl/src/misc/lv_lru.h"


static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static int DISPLAY_WIDTH;
static int DISPLAY_HEIGHT;
uint32_t *p;

static void end_frame()
{
    pb_end(p);
    while (pb_busy());
    while (pb_finished());
}

static void begin_frame()
{
    pb_wait_for_vbl();
    pb_reset();
    pb_target_back_buffer();
    while (pb_busy());
    p = pb_begin();
    p = xgu_set_color_clear_value(p, 0xff000000);
    p = xgu_clear_surface(p, XGU_CLEAR_Z | XGU_CLEAR_STENCIL | XGU_CLEAR_COLOR);
}

void lvgl_getlock(void);
void lvgl_removelock(void);

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    lvgl_removelock();
    end_frame();
    begin_frame();
    lvgl_getlock();
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(int width, int height)
{
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;

    lv_disp_draw_buf_init(&draw_buf, NULL, NULL, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_ctx_init = lv_draw_xgu_init_ctx;
    disp_drv.draw_ctx_deinit = lv_draw_xgu_deinit_ctx;
    disp_drv.draw_ctx_size = sizeof(lv_draw_xgu_ctx_t);

    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;

    lv_draw_xgu_data_t *data = lv_mem_alloc(sizeof(lv_draw_xgu_data_t));
    disp_drv.user_data = data;
    lv_disp_drv_register(&disp_drv);

    if (LV_COLOR_DEPTH == 16)
    {
        pb_set_color_format(NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5, false);
    }
    pb_init();
    pb_show_front_screen();

    p = pb_begin();

    #include "xgu/notexture.inl"
    data->combiner_mode = 0;
    data->current_tex = NULL;
    data->tex_enabled = 0;

    int widescreen = (XVideoGetEncoderSettings() & 0x00010000) ? 1 : 0;
    float x_scale =  (DISPLAY_WIDTH == 640 && widescreen == 1) ? 0.75f : 1.0f;
    float x_offset = (DISPLAY_WIDTH == 640 && widescreen == 1) ? 80.0f : 0.0f;

    float m_identity[4 * 4] = {
        x_scale, 0.0f, 0.0f, x_offset,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};

    p = xgu_set_blend_enable(p, true);
    p = xgu_set_depth_test_enable(p, true);
    p = xgu_set_blend_func_sfactor(p, XGU_FACTOR_SRC_ALPHA);
    p = xgu_set_blend_func_dfactor(p, XGU_FACTOR_ONE_MINUS_SRC_ALPHA);
    p = xgu_set_depth_func(p, XGU_FUNC_LESS_OR_EQUAL);

    p = xgu_set_skin_mode(p, XGU_SKIN_MODE_OFF);
    p = xgu_set_normalization_enable(p, false);
    p = xgu_set_lighting_enable(p, false);
    p = xgu_set_clear_rect_vertical(p, 0 , pb_back_buffer_height());
    p = xgu_set_clear_rect_horizontal(p, 0 , pb_back_buffer_width());

    for (int i = 0; i < XGU_TEXTURE_COUNT; i++)
    {
        p = xgu_set_texgen_s(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_t(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_r(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texgen_q(p, i, XGU_TEXGEN_DISABLE);
        p = xgu_set_texture_matrix_enable(p, i, false);
        p = xgu_set_texture_matrix(p, i, m_identity);
    }

    for (int i = 0; i < XGU_WEIGHT_COUNT; i++)
    {
        p = xgu_set_model_view_matrix(p, i, m_identity);
        p = xgu_set_inverse_model_view_matrix(p, i, m_identity);
    }

    p = xgu_set_transform_execution_mode(p, XGU_FIXED, XGU_RANGE_MODE_PRIVATE);
    p = xgu_set_projection_matrix(p, m_identity);
    p = xgu_set_composite_matrix(p, m_identity);
    p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
    p = xgu_set_viewport_scale(p, 1.0f, 1.0f, 1.0f, 1.0f);

    pb_end(p);
    begin_frame();
}

void lv_port_disp_deinit()
{
    lv_mem_free(disp_drv.user_data);
    while (pb_busy());
    while (pb_finished());
    pb_show_debug_screen();
    debugClearScreen();
}
