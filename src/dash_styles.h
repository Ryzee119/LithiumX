// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _STYLES_H
#define _STYLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define DASH_MENU_COLOR "#999999"

extern lv_style_t dash_background_style;
extern lv_style_t menu_table_style;
extern lv_style_t menu_table_highlight_style;
extern lv_style_t menu_table_cell_style;
extern lv_style_t synop_container_style;
extern lv_style_t synop_text_style;
extern lv_style_t titleview_style;
extern lv_style_t titleview_image_container_style;
extern lv_style_t titleview_image_text_style;
extern lv_style_t titleview_header_footer_style;
extern lv_style_t eeprom_items_style;

void dash_styles_init(void);
void dash_styles_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
