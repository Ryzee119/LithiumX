// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _DASH_SCROLLER_H
#define _DASH_SCROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lithiumx.h"

#define DASH_SORT_A_Z 0
#define DASH_SORT_RATING 1
#define DASH_SORT_LAST_LAUNCH 2
#define DASH_SORT_RELEASE_DATE 3
#define DASH_SORT_MAX 4

void dash_scroller_init(void);
void dash_scroller_scan_db(void);
void dash_scroller_set_page(void);
const char *dash_scroller_get_title(int index);
bool dash_scroller_get_sort_value(const char *page_title, int *sort_value);
void dash_scroller_resort_page(const char *page_title);
void dash_scroller_clear_page(const char *page_title);
int dash_scroller_get_page_count();
#ifdef __cplusplus
}
#endif

#endif
