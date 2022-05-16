/**
 * @file lv_draw_gl.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/src/draw/lv_draw.h"
#include "lvgl/src/misc/lv_lru.h"
#include "lvgl/src/lv_conf_internal.h"
#include "lv_draw_pbgl.h"
#include "helpers/nano_debug.h"
#include <xboxkrnl/xboxkrnl.h>
#include <pbgl.h>
#include <GL/gl.h>

typedef struct
{
    GLuint texture;
    uint32_t tw;
    uint32_t th;
    uint32_t iw;
    uint32_t ih;
} draw_cache_value_t;
lv_lru_t *texture_cache;
static const int TEXTURE_CACHE_SIZE = 16 * 1024 * 1024;

static const uint8_t _lv_bpp1_opa_table[2] = {0, 255};          /*Opacity mapping with bpp = 1 (Just for compatibility)*/
static const uint8_t _lv_bpp2_opa_table[4] = {0, 85, 170, 255}; /*Opacity mapping with bpp = 2*/

static const uint8_t _lv_bpp3_opa_table[8] = {0, 36, 73, 109, /*Opacity mapping with bpp = 3*/
                                              146, 182, 219, 255};

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

static draw_cache_value_t *create_texture(const uint8_t *src_buf, const lv_area_t *src_area, GLenum format, uint32_t bytes_pp, GLenum type, uint32_t key)
{
    uint8_t *src_pot;
    bool npot = false;

    draw_cache_value_t *texture = NULL;
    lv_lru_get(texture_cache, &key, sizeof(key), (void **)&texture);
    if (texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture->texture);
        // DbgPrint("Found cached texture with key %08x\r\n", key);
        return texture;
    }

    texture = lv_mem_alloc(sizeof(draw_cache_value_t));
    texture->iw = lv_area_get_width(src_area);
    texture->ih = lv_area_get_height(src_area);
    texture->th = npot2pot(texture->ih);
    texture->tw = npot2pot(texture->iw);
    if (texture->iw != texture->tw || texture->ih != texture->th)
    {
        npot = true;
        src_pot = lv_mem_alloc(texture->tw * texture->th * bytes_pp);
        if (src_pot == NULL)
        {
            return NULL;
        }
        uint8_t *src_tex = (uint8_t *)src_buf;
        uint8_t *dst_tex = (uint8_t *)src_pot;
        int dst_px = 0, src_px = 0;
        for (int y = 0; y < texture->ih; y++)
        {
            memcpy(&dst_tex[dst_px], &src_tex[src_px], texture->iw * bytes_pp);
            dst_px += texture->tw * bytes_pp;
            src_px += texture->iw * bytes_pp;
        }
    }
    else
    {
        src_pot = (void *)src_buf;
    }

#if (0)
    DbgPrint("Created new tex with key %08x w%d h%d %d %d\r\n", key,
             texture->iw,
             texture->ih,
             texture->tw,
             texture->th);
#endif

    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, format, texture->tw, texture->th, 0, format, type, src_pot);

    if (npot)
        lv_mem_free(src_pot);

    lv_lru_set(texture_cache, &key, sizeof(key), texture, texture->tw * texture->th * bytes_pp);

    return texture;
}

static void buffer_copy(lv_draw_ctx_t *draw_ctx,
                        void *dest_buf, lv_coord_t dest_stride, const lv_area_t *dest_area,
                        void *src_buf, lv_coord_t src_stride, const lv_area_t *src_area)
{
    LV_UNUSED(draw_ctx);

    lv_color_t *dest_bufc = dest_buf;
    lv_color_t *src_bufc = src_buf;

    /*Got the first pixel of each buffer*/
    dest_bufc += dest_stride * dest_area->y1;
    dest_bufc += dest_area->x1;

    src_bufc += src_stride * src_area->y1;
    src_bufc += src_area->x1;

    uint32_t line_length = lv_area_get_width(dest_area) * sizeof(lv_color_t);
    lv_coord_t y;
    for (y = dest_area->y1; y <= dest_area->y2; y++)
    {
        lv_memcpy(dest_bufc, src_bufc, line_length);
        dest_bufc += dest_stride;
        src_bufc += src_stride;
    }
}

/**********************
 *  STATIC VARIABLES
 **********************/
void draw_arc(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_arc_dsc_t *dsc, const lv_point_t *center,
              uint16_t radius, uint16_t start_angle, uint16_t end_angle)
{
    DbgPrint("%s\r\n", __FUNCTION__);
}

static void amask_to_rgba(uint32_t *dest, const uint8_t *src, int width, int height, int stride, uint8_t bpp)
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
    /* Does this work well on big endian systems? */
    while (cur < src_len)
    {
        curbit = 8 - bpp;
        uint8_t src_byte = src[cur * bpp / 8];
        while (curbit >= 0 && cur < src_len)
        {
            uint8_t src_bits = opa_mask & (src_byte >> curbit);
            dest[(cur / width * stride) + (cur % width)] = 0x00FFFFFF;
            dest[(cur / width * stride) + (cur % width)] |= opa_table[src_bits] << 24;
            curbit -= bpp;
            cur++;
        }
    }
}

void draw_letter(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_label_dsc_t *dsc, const lv_point_t *pos_p,
                 uint32_t letter)
{
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
    int32_t pos_y = pos_p->y + (g.resolved_font->line_height - g.resolved_font->base_line) - g.box_h - g.ofs_y;

    const lv_area_t letter_area = {pos_x, pos_y, pos_x + g.box_w - 1, pos_y + g.box_h - 1};
    lv_area_t draw_area;
    if (!_lv_area_intersect(&draw_area, &letter_area, draw_ctx->clip_area))
    {
        return;
    }

    uint8_t bytes_pp = 4;
    void *buf = lv_mem_alloc(g.box_w * g.box_h * bytes_pp);
    if (buf == NULL)
    {
        return;
    }
    amask_to_rgba(buf, bmp, g.box_w, g.box_h, g.box_w, g.bpp);
    texture = create_texture(buf, &letter_area, GL_RGBA, bytes_pp, GL_UNSIGNED_BYTE, (uint32_t)bmp);

    if (texture == NULL)
    {
        lv_mem_free(buf);
        return;
    }
    lv_mem_free(buf);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glColor4f((GLfloat)dsc->color.ch.red / 255.0f,
              (GLfloat)dsc->color.ch.green / 255.0f,
              (GLfloat)dsc->color.ch.blue / 255.0f,
              (GLfloat)dsc->opa / 255.0f);

    float tw, th, s0, s1, t0, t1;

    tw = (float)texture->iw / (float)texture->tw;
    th = (float)texture->ih / (float)texture->th;

    s0 = ((float)draw_area.x1 - (float)letter_area.x1) / (float)texture->tw;
    s1 = tw - ((((float)letter_area.x2 - (float)draw_area.x2) / texture->tw));

    t0 = ((float)draw_area.y1 - (float)letter_area.y1) / (float)texture->th;
    t1 = th - ((((float)letter_area.y2 - (float)draw_area.y2) / texture->th));

    glBegin(GL_QUADS);

    glTexCoord2f(s0, t0);
    glVertex2f(draw_area.x1, draw_area.y1);

    glTexCoord2f(s1, t0);
    glVertex2f(draw_area.x2, draw_area.y1);

    glTexCoord2f(s1, t1);
    glVertex2f(draw_area.x2, draw_area.y2);

    glTexCoord2f(s0, t1);
    glVertex2f(draw_area.x1, draw_area.y2);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_img_decoded(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_img_dsc_t *dsc,
                      const lv_area_t *src_area, const uint8_t *src_buf, lv_img_cf_t cf)
{
    draw_cache_value_t *texture = NULL;

    lv_area_t src_area_transformed;
    _lv_img_buf_get_transformed_area(&src_area_transformed, lv_area_get_width(src_area), lv_area_get_height(src_area),0,
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

    // Create a checksum of some initial data to create a unique key for the texture cache
    uint32_t key = 0;
    for (int i = 0; i < 32; i++)
    {
        uint32_t *_src = (uint32_t *)src_buf;
        key += _src[i];
    }
    texture = create_texture(src_buf, src_area, GL_RGBA, sizeof(lv_color_t), GL_UNSIGNED_BYTE, key);
    if (texture == NULL)
    {
        return;
    }

    GLint filter = (dsc->antialias) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glColor4f(1.0f, 1.0f, 1.0f, (GLfloat)dsc->opa / 255.0f);

    glBegin(GL_QUADS);

    GLfloat zm, tw, th, s0, s1, t0, t1;

    zm = (GLfloat)dsc->zoom / 256.0f;
    tw = (float)texture->iw / (float)texture->tw;
    th = (float)texture->ih / (float)texture->th;

    s0 = ((float)draw_area.x1 - (float)src_area_transformed.x1) / (float)texture->tw / zm;
    s1 = tw - ((((float)src_area_transformed.x2 - (float)draw_area.x2) / (float)texture->tw) / zm);

    t0 = ((float)draw_area.y1 - (float)src_area_transformed.y1) / (float)texture->th / zm;
    t1 = th - ((((float)src_area_transformed.y2 - (float)draw_area.y2) / (float)texture->th) / zm);

    glTexCoord2f(s0, t0);
    glVertex2f(draw_area.x1, draw_area.y1);

    glTexCoord2f(s1, t0);
    glVertex2f(draw_area.x2, draw_area.y1);

    glTexCoord2f(s1, t1);
    glVertex2f(draw_area.x2, draw_area.y2);

    glTexCoord2f(s0, t1);
    glVertex2f(draw_area.x1, draw_area.y2);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_line(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_line_dsc_t *dsc, const lv_point_t *point1,
               const lv_point_t *point2)
{
    DbgPrint("%s\r\n", __FUNCTION__);
}

void wait_for_finish(struct _lv_draw_ctx_t *draw_ctx)
{
}

static void cache_free(draw_cache_value_t *texture)
{
    glDeleteTextures(1, &texture->texture);
    lv_mem_free(texture);
}

void lv_draw_gl_init_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    LV_UNUSED(drv);

    lv_draw_gl_ctx_t *draw_gl_ctx = (lv_draw_gl_ctx_t *)draw_ctx;
    lv_memset_00(draw_gl_ctx, sizeof(lv_draw_gl_ctx_t));

    draw_gl_ctx->base_draw.draw_arc = draw_arc;
    draw_gl_ctx->base_draw.draw_rect = pbgl_draw_rect;
    draw_gl_ctx->base_draw.draw_bg = pbgl_draw_bg;
    draw_gl_ctx->base_draw.draw_letter = draw_letter;
    draw_gl_ctx->base_draw.draw_img_decoded = draw_img_decoded;
    draw_gl_ctx->base_draw.draw_line = draw_line;
    draw_gl_ctx->base_draw.draw_polygon = pbgl_draw_polygon;
    draw_gl_ctx->base_draw.wait_for_finish = wait_for_finish;
    draw_gl_ctx->base_draw.buffer_copy = buffer_copy;

    texture_cache = lv_lru_create(TEXTURE_CACHE_SIZE, 65536, (lv_lru_free_t *)cache_free, NULL);
}

void lv_draw_gl_deinit_ctx(lv_disp_drv_t *drv, lv_draw_ctx_t *draw_ctx)
{
    LV_UNUSED(drv);

    lv_draw_gl_ctx_t *draw_gl_ctx = (lv_draw_gl_ctx_t *)draw_ctx;
    lv_memset_00(draw_gl_ctx, sizeof(lv_draw_gl_ctx_t));

    lv_lru_del(texture_cache);
}
