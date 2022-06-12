// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "lvgl.h"
#include "dash.h"
#include "dash_styles.h"

lv_style_t dash_background_style;
lv_style_t menu_table_style;
lv_style_t menu_table_highlight_style;
lv_style_t menu_table_cell_style;
lv_style_t synop_container_style;
lv_style_t synop_text_style;
lv_style_t titleview_style;
lv_style_t titleview_image_container_style;
lv_style_t titleview_image_text_style;
lv_style_t titleview_header_footer_style;
lv_style_t eeprom_items_style;

void dash_styles_init(void)
{
    // Set the style for the background
    lv_style_init(&dash_background_style);
    lv_style_set_border_width(&dash_background_style, 0);
    lv_style_set_radius(&dash_background_style, 0);
    lv_style_set_bg_color(&dash_background_style, lv_color_make(15, 15, 15));
    lv_style_set_bg_grad_color(&dash_background_style, lv_color_make(22, 111, 15));
    lv_style_set_bg_grad_dir(&dash_background_style, LV_GRAD_DIR_VER);

    // Set the style for the main menu container
    lv_style_init(&menu_table_style);
    lv_style_set_bg_color(&menu_table_style, lv_color_make(15, 15, 15));
    lv_style_set_bg_opa(&menu_table_style, 240);
    lv_style_set_border_width(&menu_table_style, 1);
    lv_style_set_border_color(&menu_table_style, lv_color_make(255, 255, 255));
    lv_style_set_pad_all(&menu_table_style, 0);
    lv_style_set_radius(&menu_table_style, 0);
    lv_style_set_text_color(&menu_table_style, lv_color_white());
    lv_style_set_text_font(&menu_table_style, &lv_font_montserrat_20);
    lv_style_set_outline_width(&menu_table_style, 0);
    lv_style_set_text_line_space(&menu_table_style, 10);

    // Set the style for the main menu item cells.
    lv_style_init(&menu_table_cell_style);
    lv_style_set_border_width(&menu_table_cell_style, 1);
    lv_style_set_border_color(&menu_table_cell_style, lv_color_make(255, 255, 255));
    lv_style_set_bg_opa(&menu_table_cell_style, 0);
    lv_style_set_text_color(&menu_table_cell_style, lv_color_white());
    lv_style_set_text_font(&menu_table_cell_style, &lv_font_montserrat_20);
    lv_style_set_pad_top(&menu_table_cell_style, 10);
    lv_style_set_pad_bottom(&menu_table_cell_style, 10);
    lv_style_set_radius(&menu_table_cell_style, 0);
    lv_style_set_outline_width(&menu_table_cell_style, 0);

    // Set the style for the main menu item cells when they are selected
    lv_style_init(&menu_table_highlight_style);
    lv_style_set_bg_color(&menu_table_highlight_style, lv_color_make(61, 153, 0));

    // Set the style for the synopsis screen container
    lv_style_init(&synop_container_style);
    lv_style_set_bg_color(&synop_container_style, lv_color_make(15, 15, 15));
    lv_style_set_bg_opa(&synop_container_style, 200);
    lv_style_set_pad_left(&synop_container_style, 10);
    lv_style_set_border_width(&synop_container_style, 1);
    lv_style_set_radius(&synop_container_style, 0);

    // Set the style for the synopsis screen text.
    // To change the colour of the synop title text, edit DASH_MENU_COLOR in dash_styles.h
    lv_style_init(&synop_text_style);
    lv_style_set_text_color(&synop_text_style, lv_color_white());
    lv_style_set_text_font(&synop_text_style, &lv_font_montserrat_20);

    // Create a style for the container that holds all the images
    // Set padding between images, background colour behind the images etc
    lv_style_init(&titleview_style);
    lv_style_set_radius(&titleview_style, 0);
    lv_style_set_border_width(&titleview_style, 0);
    lv_style_set_bg_color(&titleview_style, lv_color_make(34, 34, 34));
    lv_style_set_pad_all(&titleview_style, 0);
    lv_style_set_pad_row(&titleview_style, 0);
    lv_style_set_pad_column(&titleview_style, 0);

    // Create a style for the image containers that contain an image. We basically want no borders or padding
    // as we want this container to be invisible
    lv_style_init(&titleview_image_container_style);
    lv_style_set_radius(&titleview_image_container_style, 0);
    lv_style_set_border_width(&titleview_image_container_style, 0);
    lv_style_set_bg_color(&titleview_image_container_style, lv_color_make(34, 34, 34));
    lv_style_set_border_color(&titleview_image_container_style, lv_color_make(255, 255, 255));
    lv_style_set_pad_all(&titleview_image_container_style, 0);
    lv_style_set_border_width(&titleview_image_container_style, 0);

    // Create a style for the text that appears on the thumbnail art when no artwork is found
    lv_style_init(&titleview_image_text_style);
    lv_style_set_align(&titleview_image_text_style, LV_ALIGN_CENTER);
    lv_style_set_text_font(&titleview_image_text_style, &lv_font_montserrat_20);
    lv_style_set_text_color(&titleview_image_text_style, lv_color_white());
    lv_style_set_text_align(&titleview_image_text_style, LV_TEXT_ALIGN_CENTER);

    // Create a style for the page header and footer text
    lv_style_init(&titleview_header_footer_style);
    lv_style_set_bg_color(&titleview_header_footer_style, lv_color_make(0, 72, 16));
    lv_style_set_text_color(&titleview_header_footer_style, lv_color_white());
    lv_style_set_text_font(&titleview_header_footer_style, &lv_font_montserrat_26);

    //Style for the eeprom setting screen
    lv_style_init(&eeprom_items_style);
    lv_style_set_border_width(&eeprom_items_style, 1);
    lv_style_set_border_color(&eeprom_items_style, lv_color_white());
    lv_style_set_bg_color(&eeprom_items_style, lv_color_make(15, 15, 15));

    lv_style_set_text_color(&eeprom_items_style, lv_color_white());
    lv_style_set_text_font(&eeprom_items_style, &lv_font_montserrat_20);
    lv_style_set_color_filter_opa(&eeprom_items_style, 0);
    lv_style_set_radius(&eeprom_items_style, 0);
    lv_style_set_outline_width(&eeprom_items_style, 0);
}

void dash_styles_deinit(void)
{
    lv_style_reset(&dash_background_style);
    lv_style_reset(&menu_table_style);
    lv_style_reset(&menu_table_highlight_style);
    lv_style_reset(&menu_table_cell_style);
    lv_style_reset(&synop_container_style);
    lv_style_reset(&synop_text_style);
    lv_style_reset(&titleview_style);
    lv_style_reset(&titleview_image_container_style);
    lv_style_reset(&titleview_image_text_style);
    lv_style_reset(&titleview_header_footer_style);
    lv_style_reset(&eeprom_items_style);
}
