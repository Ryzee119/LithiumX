// SPDX-License-Identifier: MIT

#include "lv_xgu_draw.h"
#include "src/draw/lv_draw.h"
#include "libs/xgu/xgu.h"
#include "libs/xgu/xgux.h"
#include "src/misc/lv_lru.h"

extern uint32_t *p;

static const uint8_t _lv_bpp1_opa_table[2] = {0, 255};          /*Opacity mapping with bpp = 1 (Just for compatibility)*/

static const uint8_t _lv_bpp2_opa_table[4] = {0, 85, 170, 255}; /*Opacity mapping with bpp = 2*/

static const uint8_t _lv_bpp4_opa_table[16] = {0, 17, 34, 51, /*Opacity mapping with bpp = 4*/
                                               68, 85, 102, 119,
                                               136, 153, 170, 187,
                                               204, 221, 238, 255};

static const uint8_t _lv_bpp8_opa_table[256] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                                16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                                                32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                                                48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
                                                64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
                                                80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
                                                96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
                                                112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
                                                128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
                                                144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
                                                160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
                                                176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
                                                192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
                                                208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
                                                224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
                                                240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};

static void amask_to_a(uint8_t *dest, const uint8_t *src, int width, int height, int stride, uint8_t bpp)
{
    int src_len = width * height;
    int cur = 0;
    int curbit;
    uint8_t opa_mask;
    const uint8_t *opa_table;
    switch (bpp)
    {
    case 1:
        opa_mask = 0x1;
        opa_table = _lv_bpp1_opa_table;
        break;
    case 2:
        opa_mask = 0x4;
        opa_table = _lv_bpp2_opa_table;
        break;
    case 4:
        opa_mask = 0xF;
        opa_table = _lv_bpp4_opa_table;
        break;
    case 8:
        opa_mask = 0xFF;
        opa_table = _lv_bpp8_opa_table;
        break;
    default:
        return;
    }

    while (cur < src_len)
    {
        curbit = 8 - bpp;
        uint8_t src_byte = src[cur * bpp / 8];
        while (curbit >= 0 && cur < src_len)
        {
            uint8_t src_bits = opa_mask & (src_byte >> curbit);
            dest[(cur / width * stride) + (cur % width)] = opa_table[src_bits];
            curbit -= bpp;
            cur++;
        }
    }
}

static void bind_texture(lv_draw_xgu_ctx_t *xgu_ctx, draw_cache_value_t *texture, uint32_t tex_id, XguTexFilter filter)
{
    if (xgu_ctx->xgu_data->current_tex != tex_id)
    {
        p = xgu_set_texture_offset(p, 0, (void *)MmGetPhysicalAddress(texture->texture));
        p = xgu_set_texture_format(p, 0, 2, false, XGU_SOURCE_COLOR, 2, texture->format, 1, texture->tw >> 8, texture->th >> 8, 0);
        p = xgu_set_texture_address(p, 0, XGU_CLAMP_TO_EDGE, false, XGU_CLAMP_TO_EDGE, false, XGU_CLAMP_TO_EDGE, false, false);
        p = xgu_set_texture_control0(p, 0, true, 0, 0);
        p = xgu_set_texture_control1(p, 0, texture->tw * texture->bytes_pp);
        p = xgu_set_texture_image_rect(p, 0, texture->tw, texture->th);
        p = xgu_set_texture_filter(p, 0, 0, XGU_TEXTURE_CONVOLUTION_GAUSSIAN, filter, filter, false, false, false, false);
        xgu_ctx->xgu_data->current_tex = tex_id;
    }
}

static void *create_texture(lv_draw_xgu_ctx_t *xgu_ctx, const uint8_t *src_buf, const lv_area_t *src_area, XguTexFormatColor fmt, uint32_t bytes_pp, uint32_t key)
{
    draw_cache_value_t *texture = NULL;
    uint32_t iw = lv_area_get_width(src_area);
    uint32_t ih = lv_area_get_height(src_area);
    uint32_t tw = npot2pot(iw);
    uint32_t th = npot2pot(ih);
    uint32_t sz;
    //Seems like there's a min texture size of 8 bytes.
    //Fix me, small textures will still use a whole page of memory.
    tw = LV_MAX(tw, 8 / bytes_pp);
    th = LV_MAX(th, 8 / bytes_pp);
    sz = tw * th * bytes_pp;

    //Allocate it in cache
    texture = lv_mem_alloc(sizeof(draw_cache_value_t));
    texture->iw = iw;
    texture->ih = ih;
    texture->tw = tw;
    texture->th = th;
    texture->format = fmt;
    texture->bytes_pp = bytes_pp;
    lv_lru_set(xgu_ctx->xgu_data->texture_cache, &key, sizeof(key), texture, sz + (sz % PAGE_SIZE));

    uint8_t *dst_buf = (uint8_t *)MmAllocateContiguousMemoryEx(sz, 0, 0xFFFFFFFF, 0,
                                                               PAGE_WRITECOMBINE | PAGE_READWRITE);
    if (dst_buf == NULL)
    {
        lv_lru_remove(xgu_ctx->xgu_data->texture_cache, &key, sizeof(key));
        lv_mem_free(texture);
        return NULL;
    }
    texture->texture = dst_buf;

    uint32_t dst_px = 0, src_px = 0;
    for (int y = 0; y < ih; y++)
    {
        memcpy(&dst_buf[dst_px], &src_buf[src_px], iw * bytes_pp);
        dst_px += tw * bytes_pp;
        src_px += iw * bytes_pp;
    }

    return texture;
}

static void map_textured_rect(draw_cache_value_t *texture, const lv_area_t *tex_area,
                              lv_area_t *draw_area, float zoom)
{
    float zm, s0, s1, t0, t1;
    zm = zoom / 256.0f;
    s0 = (float)(draw_area->x1 - tex_area->x1) / zm;
    s1 = texture->iw - ((float)(tex_area->x2 - draw_area->x2) / zm);
    t0 = (float)(draw_area->y1 - tex_area->y1) / zm;
    t1 = texture->ih - ((float)(tex_area->y2 - draw_area->y2) / zm);

    p = xgu_begin(p, XGU_TRIANGLE_STRIP);

    p = xgux_set_texcoord3f(p, 0, s0, t0, 1);
    p = xgu_vertex4f(p, (float)draw_area->x1, (float)draw_area->y1, 1, 1);

    p = xgux_set_texcoord3f(p, 0, s1, t0, 1);
    p = xgu_vertex4f(p, (float)draw_area->x2, (float)draw_area->y1, 1, 1);

    p = xgux_set_texcoord3f(p, 0, s0, t1, 1);
    p = xgu_vertex4f(p, (float)draw_area->x1, (float)draw_area->y2, 1, 1);

    p = xgux_set_texcoord3f(p, 0, s1, t1, 1);
    p = xgu_vertex4f(p, (float)draw_area->x2, (float)draw_area->y2, 1, 1);

    p = xgu_end(p);
}

void xgu_draw_letter(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_label_dsc_t *dsc,
                     const lv_point_t *pos_p, uint32_t letter)
{
    lv_draw_xgu_ctx_t *xgu_ctx = (lv_draw_xgu_ctx_t *)draw_ctx;
    draw_cache_value_t *texture = NULL;
    lv_font_glyph_dsc_t g;

    if (dsc->opa < LV_OPA_MIN)
    {
        return;
    }

    if (lv_font_get_glyph_dsc(dsc->font, &g, letter, '\0') == false)
    {
        return;
    }

    if ((g.box_h == 0) || (g.box_w == 0))
    {
        return;
    }

    const uint8_t *bmp = lv_font_get_glyph_bitmap(g.resolved_font, letter);
    if (bmp == NULL)
    {
        return;
    }

    int32_t pos_x = pos_p->x + g.ofs_x;
    int32_t pos_y = pos_p->y + (g.resolved_font->line_height - g.resolved_font->base_line) -
                    g.box_h - g.ofs_y;

    const lv_area_t letter_area = {pos_x, pos_y, pos_x + g.box_w - 1, pos_y + g.box_h - 1};
    lv_area_t draw_area;
    if (!_lv_area_intersect(&draw_area, &letter_area, draw_ctx->clip_area))
    {
        return;
    }

    if (xgu_ctx->xgu_data->combiner_mode != 1)
    {
        #include "lvgl_drivers/video/xgu/texture.inl"
        xgu_ctx->xgu_data->combiner_mode = 1;
    }

    lv_lru_get(xgu_ctx->xgu_data->texture_cache, &bmp, sizeof(bmp), (void **)&texture);
    if (texture == NULL)
    {
        uint8_t bytes_pp = 1;
        void *buf = lv_mem_alloc(g.box_w * g.box_h * bytes_pp);
        if (buf == NULL)
        {
            return;
        }
        amask_to_a(buf, bmp, g.box_w, g.box_h, g.box_w, g.bpp);
        texture = create_texture(xgu_ctx, buf, &letter_area,
                                 XGU_TEXTURE_FORMAT_A8, bytes_pp, (uint32_t)bmp);

        lv_mem_free(buf);
        if (texture == NULL)
        {
            return;
        }
    }

    p = xgux_set_color4ub(p, dsc->color.ch.red, dsc->color.ch.green,
                          dsc->color.ch.blue, dsc->opa);

    bind_texture(xgu_ctx, texture, (uint32_t)bmp, XGU_TEXTURE_FILTER_LINEAR);

    map_textured_rect(texture, &letter_area, &draw_area, 256.0f);
}

lv_res_t xgu_draw_img(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_img_dsc_t *dsc,
                      const lv_area_t *src_area, const void *src_buf)
{
    lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)src_buf;
    lv_img_src_t src_type = lv_img_src_get_type(src_buf);

    if (src_type != LV_IMG_SRC_VARIABLE)
    {
        DbgPrint("Unsupported img type %d\n", src_type);
        return LV_RES_INV;
    }

    switch (img_dsc->header.cf)
    {
    case LV_IMG_CF_TRUE_COLOR:
    case LV_IMG_CF_RGB888:
    case LV_IMG_CF_RGBA8888:
    case LV_IMG_CF_RGBX8888:
    case LV_IMG_CF_RGB565:
    case LV_IMG_CF_INDEXED_1BIT:
        break;
    case LV_IMG_CF_TRUE_COLOR_ALPHA:
        if (sizeof(lv_color_t) == 4) break;
        //Fallthrough on other than 32bpp
    default:
        DbgPrint("Unsupported texture format %d\n", img_dsc->header.cf);
        return LV_RES_INV;
    }
    xgu_draw_img_decoded(draw_ctx, dsc, src_area, img_dsc->data, img_dsc->header.cf);
    return LV_RES_OK;
}

void draw_rect_simple(const lv_area_t *draw_area);
void xgu_draw_img_decoded(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_img_dsc_t *dsc,
                          const lv_area_t *src_area, const uint8_t *src_buf, lv_img_cf_t cf)
{
    lv_draw_xgu_ctx_t *xgu_ctx = (lv_draw_xgu_ctx_t *)draw_ctx;
    draw_cache_value_t *texture = NULL;

    lv_area_t src_area_transformed;
    _lv_img_buf_get_transformed_area(&src_area_transformed,
                                     lv_area_get_width(src_area),
                                     lv_area_get_height(src_area),
                                     0,
                                     dsc->zoom, &dsc->pivot);
    lv_area_move(&src_area_transformed, src_area->x1, src_area->y1);

    lv_area_t draw_area;
    if (!_lv_area_intersect(&draw_area, &src_area_transformed, draw_ctx->clip_area))
    {
        return;
    }

    if (draw_area.x1 == draw_area.x2)
        draw_area.x2++;
    if (draw_area.y1 == draw_area.y2)
        draw_area.y2++;

    lv_color_t recolor = lv_color_make(255, 255, 255);

    // If we are about the draw 1 bit indexed image. Setup draw color froms src_buf;
    if (cf == LV_IMG_CF_INDEXED_1BIT)
    {
        // Draw background
        lv_color_t *c2 = (lv_color_t *)&src_buf[0];
        p = xgux_set_color4ub(p, c2->ch.red, c2->ch.green,
                                 c2->ch.blue, 0xFF);
        draw_rect_simple(src_area);

        // Prep foreground
        lv_color_t *c1 = (lv_color_t *)&src_buf[4];
        recolor.ch.red = c1->ch.red;
        recolor.ch.green = c1->ch.green;
        recolor.ch.blue = c1->ch.blue;
    }

    // Create a checksum of some initial data to create a unique key for the texture cache
    uint32_t key = 0;
    uint32_t max = (lv_area_get_width(src_area) *
               lv_area_get_height(src_area) * sizeof(lv_color_t)) / 4;
    uint32_t *_src = (uint32_t *)src_buf;
    int i = 0, end = LV_MIN(i + 16, max);
    while (i < end) key += _src[i++];
    i = max / 2; end = LV_MIN(i + 16, max);
    while (i < end) key += _src[i++];
    if (xgu_ctx->xgu_data->combiner_mode != 1)
    {
        #include "lvgl_drivers/video/xgu/texture.inl"
        xgu_ctx->xgu_data->combiner_mode = 1;
    }

    lv_lru_get(xgu_ctx->xgu_data->texture_cache, &key, sizeof(key), (void **)&texture);
    if (texture == NULL)
    {
        XguTexFormatColor xgu_cf;
        uint8_t bytes_pp;
        switch (cf)
        {
        case LV_IMG_CF_TRUE_COLOR:
            xgu_cf = (sizeof(lv_color_t) == 2) ? 
                XGU_TEXTURE_FORMAT_R5G6B5 : XGU_TEXTURE_FORMAT_A8R8G8B8;
            bytes_pp = sizeof(lv_color_t);
            break;
        case LV_IMG_CF_TRUE_COLOR_ALPHA:
            if (sizeof(lv_color_t) == 2) return;
            xgu_cf = XGU_TEXTURE_FORMAT_A8R8G8B8;
            bytes_pp = sizeof(lv_color_t);
        case LV_IMG_CF_RGB888:
            xgu_cf = XGU_TEXTURE_FORMAT_X8R8G8B8;
            bytes_pp = 4;
            break;
        case LV_IMG_CF_RGBA8888:
        case LV_IMG_CF_RGBX8888:
            xgu_cf = XGU_TEXTURE_FORMAT_R8G8B8A8;
            bytes_pp = 4;
            break;
        case LV_IMG_CF_RGB565:
            xgu_cf = XGU_TEXTURE_FORMAT_R5G6B5;
            bytes_pp = 2;
            break;
        case LV_IMG_CF_INDEXED_1BIT:
            xgu_cf = XGU_TEXTURE_FORMAT_A8;
            bytes_pp = 1;
            int w = lv_area_get_width(src_area);
            int h = lv_area_get_height(src_area);
            void *buf = lv_mem_alloc(w * h * bytes_pp);
            if (buf == NULL)
            {
                return;
            }
            amask_to_a(buf, &src_buf[8], w, h, w, bytes_pp);
            src_buf = buf;
            break;
        default:
            DbgPrint("Unsupported texture format %d\n", cf);
            return;
        }
        texture = create_texture(xgu_ctx, src_buf, src_area, xgu_cf, bytes_pp, (uint32_t)key);
        if (cf == LV_IMG_CF_INDEXED_1BIT)
        {
            lv_mem_free((void *)src_buf);
        }
        if (texture == NULL)
        {
            return;
        }
    }

    if (dsc->recolor_opa > LV_OPA_TRANSP)
    {
        p = xgux_set_color4ub(p, dsc->recolor.ch.red, dsc->recolor.ch.green,
                                 dsc->recolor.ch.blue, dsc->recolor_opa);
    }
    else
    {
        p = xgux_set_color4ub(p, recolor.ch.red, recolor.ch.green, recolor.ch.blue, 255);
    }

    bind_texture(xgu_ctx, texture, (uint32_t)key,
                 (dsc->antialias) ? XGU_TEXTURE_FILTER_LINEAR : XGU_TEXTURE_FILTER_NEAREST);

    map_textured_rect(texture, &src_area_transformed, &draw_area, (float)dsc->zoom);
}
