// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_H
#define _DASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <lvgl.h>
#ifdef NXDK
#include <SDL.h>
#include <nxdk/mount.h>
int strcasecmp(const char *s1, const char *s2);
#else
#include <SDL2/SDL.h>
#endif

#include "dash_database.h"
#include "dash_eeprom.h"
#include "dash_mainmenu.h"
#include "dash_scroller.h"
#include "dash_settings.h"
#include "dash_styles.h"
#include "dash_synop.h"
#include "dash_browser.h"

#include "lvgl_drivers/lv_port_disp.h"
#include "lvgl_drivers/lv_port_indev.h"
#include "lvgl_widgets/menu.h"
#include "lvgl_widgets/generic_container.h"
#include "lvgl_widgets/confirmbox.h"

#include "libs/toml/toml.h"
#include "libs/sqlite3/sqlite3.h"
#include "libs/jpg_decoder/jpg_decoder.h"
#include "libs/sxml/sxml.h"
#include "libs/toml/toml.h"
#include "libs/tlsf/tlsf.h"
#include "platform/platform.h"

/// Macro that returns the minimum of two numbers
#define DASH_MIN(_x, _y) (((_x) < (_y)) ? (_x) : (_y))

/// Macro that returns the maximum of two numbers
#define DASH_MAX(_x, _y) (((_x) > (_y)) ? (_x) : (_y))

/// Macro that returns the size of a static array
#define DASH_ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

#ifdef WIN32
#define DASH_PATH_SEPARATOR '\\'
#else
#define DASH_PATH_SEPARATOR '/'
#endif

//All lvgl directories are prefixed with Q:
//nxdk local directory is also mounting to Q: so we get Q:Q:..
#ifndef DASH_SEARCH_PATH_CONFIG
#ifdef NXDK
#define DASH_SEARCH_PATH_CONFIG "E:\\UDATA\\LithiumX\\lithiumx.toml"
#else
#define DASH_SEARCH_PATH_CONFIG "lithiumx.toml"
#endif
#endif

#ifndef DASH_DATABASE_PATH
#ifdef NXDK
#define DASH_DATABASE_PATH "E:\\UDATA\\LithiumX\\lithiumx.db"
#else
#define DASH_DATABASE_PATH "lithiumx.db"
#endif
#endif

#ifndef DASH_ROOT_PATH
#ifdef NXDK
#define DASH_ROOT_PATH ""
#else
#define DASH_ROOT_PATH "."
#endif
#endif

#ifndef DASH_LAUNCH_EXE
#define DASH_LAUNCH_EXE "default.xbe"
#endif

#ifndef DASH_MAX_PAGES
#define DASH_MAX_PAGES 8
#endif

#ifndef DASH_MAX_PATHS_PER_PAGE
#define DASH_MAX_PATHS_PER_PAGE 16
#endif

#ifndef DASH_MAX_GAMES
#define DASH_MAX_GAMES 1024 //Per page
#endif

#ifndef DASH_MAX_PATHLEN
#define DASH_MAX_PATHLEN 256 //Per page
#endif

#ifndef DASH_THUMBNAIL_WIDTH
#define DASH_THUMBNAIL_WIDTH (LV_MIN(200, (lv_obj_get_width(lv_scr_act()) - (2 * DASH_XMARGIN)) / 4))
#endif

#ifndef DASH_THUMBNAIL_HEIGHT
#define DASH_THUMBNAIL_HEIGHT (DASH_THUMBNAIL_WIDTH * 1.4)
#endif

#ifndef DASH_DEFAULT_THUMBNAIL
#define DASH_DEFAULT_THUMBNAIL "default_tbn.jpg" //Root directory if not found in game directory
#endif

#ifndef DASH_GAME_THUMBNAIL
#define DASH_GAME_THUMBNAIL "default.tbn"
#endif

#ifndef DASH_MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH 255
#endif
#define DASH_MAX_PATH MAX_PATH
#endif

#define DASH_NEXT_PAGE '>'
#define DASH_PREV_PAGE '<'
#define DASH_SETTINGS_PAGE 's'
#define DASH_INFO_PAGE 'i'

// There is one 'parser' per 'tile'. The parser asynchronously parses all the path set my the xml and adds
// eatch item. Each parser contains a image scrolling container 'scroller' to show all the game art etc.
// Each 'tile' is a child of a tileview object 'pagetiles'. These are swiped left and right to change page.
typedef struct
{
    char page_title[32];
    void *db_scan_thread;
    lv_obj_t *tile;     // The tile in the tileview parent 'pagetiles'
    lv_obj_t *scroller; // The scroller contains image containers for each item
} parse_handle_t;

typedef struct
{
    char *thumb_path;
    lv_obj_t *canvas;
    void *decomp_handle;
    void *mem; //Memory is the allocated block from malloc
    void *image; //image is the decompressed image with mem (this may be byte aligned etc)
    int w;
    int h;
} jpg_info_t;

typedef struct
{
    int db_id;
    char title[MAX_META_LEN];
    jpg_info_t *jpg_info;
} title_t;

#ifndef NANO_DEBUG_LEVEL
#define NANO_DEBUG_LEVEL LEVEL_WARN
#endif

typedef enum{

    LEVEL_TRACE,
    LEVEL_WARN,
    LEVEL_ERROR,
    LEVEL_NONE
} dash_debug_level_t;

void dash_printf(dash_debug_level_t level, const char *format, ...);

void dash_init(void);
void dash_create();
void dash_deinit(void);
void lvgl_getlock(void);
void lvgl_removelock(void);
void *lx_mem_alloc(size_t size);
void *lx_mem_realloc(void *data, size_t new_size);
void lx_mem_free(void *data);

void dash_focus_set_final(lv_obj_t *focus);
void dash_focus_change_depth(lv_obj_t *new_focus);
lv_obj_t *dash_focus_pop_depth();
void dash_focus_change(lv_obj_t *new_obj);


extern toml_table_t *dash_search_paths;
extern int settings_use_fahrenheit;
extern int settings_auto_launch_dvd;
extern int settings_default_page_index;
extern int settings_theme_colour;
extern int settings_max_recent;
extern char settings_page_sorts_str[4096];

extern const char *dash_launch_path;
extern char settings_earliest_recent_date[20];

#ifdef __cplusplus
}
#endif

#endif