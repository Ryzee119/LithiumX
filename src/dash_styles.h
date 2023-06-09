// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _STYLES_H
#define _STYLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

#define DASH_MENU_COLOR "#999999"

#ifndef DASH_XMARGIN
#define DASH_XMARGIN 20
#endif
#ifndef DASH_YMARGIN
#define DASH_YMARGIN 20
#endif

extern lv_color_t dash_base_theme_color;
extern lv_style_t dash_background_style;
extern lv_style_t menu_table_style;
extern lv_style_t menu_table_highlight_style;
extern lv_style_t menu_table_cell_style;
extern lv_style_t object_style;
extern lv_style_t titleview_style;
extern lv_style_t titleview_image_container_style;
extern lv_style_t titleview_image_text_style;
extern lv_style_t titleview_header_footer_style;

void dash_styles_init(lv_color_t theme_colour);
void dash_styles_deinit(void);

#ifdef __cplusplus
}
#endif

#endif