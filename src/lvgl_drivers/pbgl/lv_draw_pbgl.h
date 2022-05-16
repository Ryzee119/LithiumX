/**
 * @file lv_draw_pbgl.h
 *
 */

#ifndef lv_draw_pbgl_H
#define lv_draw_pbgl_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/src/draw/lv_draw.h"
#include "lvgl/src/core/lv_disp.h"

typedef struct {
    lv_draw_ctx_t base_draw;
} lv_draw_gl_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
#define SKIP_BORDER(dsc) ((dsc)->border_opa <= LV_OPA_MIN || (dsc)->border_width == 0 || (dsc)->border_side == LV_BORDER_SIDE_NONE || (dsc)->border_post)
#define SKIP_SHADOW(dsc) ((dsc)->shadow_width == 0 || (dsc)->shadow_opa <= LV_OPA_MIN || ((dsc)->shadow_width == 1 && (dsc)->shadow_spread <= 0 && (dsc)->shadow_ofs_x == 0 && (dsc)->shadow_ofs_y == 0))
#define SKIP_IMAGE(dsc) ((dsc)->bg_img_src == NULL || (dsc)->bg_img_opa <= LV_OPA_MIN)
#define SKIP_OUTLINE(dsc) ((dsc)->outline_opa <= LV_OPA_MIN || (dsc)->outline_width == 0)

static inline int npot2pot(int num)
{
    if (num != 0)
    {
        num--;
        num |= (num >> 1);  // Or first 2 bits
        num |= (num >> 2);  // Or next 2 bits
        num |= (num >> 4);  // Or next 4 bits
        num |= (num >> 8);  // Or next 8 bits
        num |= (num >> 16); // Or next 16 bits
        num++;
    }
    return num;
}

void lv_draw_gl_init_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx);
void lv_draw_gl_deinit_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx);

//Rect types
void pbgl_draw_rect(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *coords);
void pbgl_draw_bg(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *draw_dsc, const lv_area_t *coords);
void pbgl_draw_polygon(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *draw_dsc,
                  const lv_point_t *points, uint16_t point_cnt);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*lv_draw_pbgl_H*/