// SPDX-License-Identifier: MIT

#include "lv_xgu_draw.h"
#include "src/draw/lv_draw.h"
#include "libs/xgu/xgu.h"
#include "libs/xgu/xgux.h"
#include "src/misc/lv_lru.h"

#include <xboxkrnl/xboxkrnl.h>

int lv_texture_cache_size = 2 * 1024 * 1024;

static void cache_free(draw_cache_value_t *texture)
{
    MmFreeContiguousMemory(texture->texture);
    lv_mem_free(texture);
}

void xgu_draw_arc(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_arc_dsc_t *dsc, const lv_point_t *center,
                  uint16_t radius, uint16_t start_angle, uint16_t end_angle)
{
    DbgPrint("%s - not supported\r\n", __FUNCTION__);
}

void xgu_draw_line(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_line_dsc_t *dsc, const lv_point_t *point1,
                   const lv_point_t *point2)
{
    DbgPrint("%s - not supported\r\n", __FUNCTION__);
}

void lv_draw_xgu_init_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    LV_UNUSED(drv);

    lv_draw_xgu_ctx_t *xgu_ctx = (lv_draw_xgu_ctx_t *)draw_ctx;
    lv_memset_00(xgu_ctx, sizeof(lv_draw_xgu_ctx_t));

    xgu_ctx->base_draw.draw_arc = xgu_draw_arc;
    xgu_ctx->base_draw.draw_rect = xgu_draw_rect;
    xgu_ctx->base_draw.draw_bg = xgu_draw_bg;
    xgu_ctx->base_draw.draw_letter = xgu_draw_letter;
    xgu_ctx->base_draw.draw_img = xgu_draw_img;
    xgu_ctx->base_draw.draw_img_decoded = xgu_draw_img_decoded;
    xgu_ctx->base_draw.draw_line = xgu_draw_line;
    xgu_ctx->base_draw.draw_polygon = xgu_draw_polygon;
    xgu_ctx->xgu_data = drv->user_data;
    xgu_ctx->xgu_data->texture_cache = lv_lru_create(lv_texture_cache_size, 65536, (lv_lru_free_t *)cache_free, NULL);
}

void lv_draw_xgu_deinit_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    LV_UNUSED(drv);
    lv_draw_xgu_ctx_t *xgu_ctx = (lv_draw_xgu_ctx_t *)draw_ctx;

    lv_lru_del(xgu_ctx->xgu_data->texture_cache);
    lv_memset_00(xgu_ctx, sizeof(lv_draw_xgu_ctx_t));
}
