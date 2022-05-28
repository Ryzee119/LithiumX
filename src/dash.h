// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_H
#define _DASH_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define DASH_PATH_SEPARATOR '\\'
#else
#define DASH_PATH_SEPARATOR '/'
#endif

#ifndef DASH_XMARGIN
#define DASH_XMARGIN 20
#endif
#ifndef DASH_YMARGIN
#define DASH_YMARGIN 20
#endif


//All lvgl directories are prefixed with A:
//nxdk local directory is also mounting to A: so we get A:A:..
#ifndef DASH_XML
#ifdef NXDK
#define DASH_XML "A:A:\\dash.xml"
#else
#define DASH_XML "A:dash.xml"
#endif
#endif

#ifndef RECENT_TITLES
#ifdef NXDK
#define RECENT_TITLES "A:E:\\UDATA\\LithiumX\\recent.dat"
#else
#define RECENT_TITLES "A:recent.dat"
#endif
#endif

#ifndef RECENT_TITLES_MAX
#define RECENT_TITLES_MAX 16
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

#define DASH_NEXT_PAGE '>'
#define DASH_PREV_PAGE '<'
#define DASH_SETTINGS_PAGE 'S'
#define DASH_INFO_PAGE 'I'

extern unsigned int settings_use_fahrenheit;
extern unsigned int settings_default_screen_index;
extern unsigned int settings_auto_launch_dvd;

void dash_init(void);
void dash_deinit(void);
void dash_set_launch_exe(const char *format, ...);
void dash_clear_recent_list(void);
const char *dash_get_launch_exe(void);
void dash_launch_title();

void lvgl_getlock(void);
void lvgl_removelock(void);

#ifdef __cplusplus
}
#endif

#endif
