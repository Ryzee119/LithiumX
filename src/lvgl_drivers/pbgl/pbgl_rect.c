#include <pbgl.h>
#include <GL/gl.h>
#include <xboxkrnl/xboxkrnl.h>
#include "lv_draw_pbgl.h"

static void draw_rect_simple(const lv_area_t *draw_area)
{
    glBegin(GL_QUADS);
    glVertex3f(draw_area->x1, draw_area->y1, 0);
    glVertex3f(draw_area->x2, draw_area->y1, 0);
    glVertex3f(draw_area->x2, draw_area->y2, 0);
    glVertex3f(draw_area->x1, draw_area->y2, 0);
    glEnd();
}

static void rect_draw_border(const lv_area_t *draw_area, const lv_draw_rect_dsc_t *dsc)
{
    if (SKIP_BORDER(dsc))
    {
        return;
    }

    GLfloat r = (GLfloat)dsc->border_color.ch.red / 255.0f;
    GLfloat g = (GLfloat)dsc->border_color.ch.green / 255.0f;
    GLfloat b = (GLfloat)dsc->border_color.ch.blue / 255.0f;
    GLfloat a = (GLfloat)dsc->border_opa / 255.0f;
    glColor4f(r, g, b, a);

    //FIXME, what about polygons
    lv_area_t border_quad;
    if (dsc->border_side & LV_BORDER_SIDE_TOP)
    {
        border_quad.x1 = draw_area->x1;
        border_quad.x2 = draw_area->x2;
        border_quad.y1 = draw_area->y1;
        border_quad.y2 = draw_area->y1 + dsc->border_width;
        draw_rect_simple(&border_quad);
    }
    if (dsc->border_side & LV_BORDER_SIDE_LEFT)
    {
        border_quad.x1 = draw_area->x1;
        border_quad.x2 = draw_area->x1 + dsc->border_width;
        border_quad.y1 = draw_area->y1;
        border_quad.y2 = draw_area->y2;
        draw_rect_simple(&border_quad);
    }
    if (dsc->border_side & LV_BORDER_SIDE_BOTTOM)
    {
        border_quad.x1 = draw_area->x1;
        border_quad.x2 = draw_area->x2;
        border_quad.y1 = draw_area->y2 - dsc->border_width;
        border_quad.y2 = draw_area->y2;
        draw_rect_simple(&border_quad);
    }
    if (dsc->border_side & LV_BORDER_SIDE_RIGHT)
    {
        border_quad.x1 = draw_area->x2 - dsc->border_width;
        border_quad.x2 = draw_area->x2;
        border_quad.y1 = draw_area->y1;
        border_quad.y2 = draw_area->y2;
        draw_rect_simple(&border_quad);
    }
}

static void rect_draw_shadow(const lv_area_t *draw_area, const lv_draw_rect_dsc_t *dsc)
{
    if (SKIP_SHADOW(dsc))
    {
        return;
    }
    DbgPrint("%s\r\n", __FUNCTION__);
    /*
    lv_color_t shadow_color;
    lv_coord_t shadow_width;
    lv_coord_t shadow_ofs_x;
    lv_coord_t shadow_ofs_y;
    lv_coord_t shadow_spread;
    lv_opa_t shadow_opa;
    lv_color_t shadow_color
    */
    // FIXME
}

static void rect_draw_image(const lv_area_t *draw_area, const lv_draw_rect_dsc_t *dsc)
{
    if (SKIP_IMAGE(dsc))
    {
        return;
    }
    DbgPrint("%s\r\n", __FUNCTION__);
    /*
    const void * bg_img_src;
    const void * bg_img_symbol_font;
    lv_color_t bg_img_recolor;
    lv_opa_t bg_img_opa;
    lv_opa_t bg_img_recolor_opa;
    uint8_t bg_img_tiled;
    */
    // FIXME
}

static void rect_draw_outline(const lv_area_t *draw_area, const lv_draw_rect_dsc_t *dsc)
{
    if (SKIP_OUTLINE(dsc))
    {
        return;
    }
    DbgPrint("%s %d\r\n", __FUNCTION__, dsc->outline_width);
    /*
    lv_color_t outline_color;
    lv_coord_t outline_width;
    lv_coord_t outline_pad;
    lv_opa_t outline_opa;
    */
    // FIXME
}

void pbgl_draw_rect(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *dsc, const lv_area_t *src_area)
{
    lv_area_t draw_area;
    if (!_lv_area_intersect(&draw_area, src_area, draw_ctx->clip_area))
    {
        return;
    }

    rect_draw_shadow(&draw_area, dsc);
    rect_draw_outline(&draw_area, dsc);

    if (dsc->bg_opa > LV_OPA_MIN)
    {
        GLfloat r = (GLfloat)dsc->bg_color.ch.red / 255.0f;
        GLfloat g = (GLfloat)dsc->bg_color.ch.green / 255.0f;
        GLfloat b = (GLfloat)dsc->bg_color.ch.blue / 255.0f;
        GLfloat a = (GLfloat)dsc->bg_opa / 255.0f;
        glColor4f(r, g, b, a);
        draw_rect_simple(&draw_area);
    }

    rect_draw_image(&draw_area, dsc);
    rect_draw_border(&draw_area, dsc);
}

void pbgl_draw_bg(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *draw_dsc, const lv_area_t *src_area)
{
    DbgPrint("%s\r\n", __FUNCTION__);
    lv_draw_gl_ctx_t *ctx = (lv_draw_gl_ctx_t *)draw_ctx;
}

void pbgl_draw_polygon(struct _lv_draw_ctx_t *draw_ctx, const lv_draw_rect_dsc_t *draw_dsc,
                  const lv_point_t *points, uint16_t point_cnt)
{
    DbgPrint("%s\r\n", __FUNCTION__);
    lv_draw_gl_ctx_t *ctx = (lv_draw_gl_ctx_t *)draw_ctx;
}