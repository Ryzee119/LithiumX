// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#ifndef _WIDGET_CONFIRMBOX_H
#define _WIDGET_CONFIRMBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lithiumx.h"

lv_obj_t *confirmbox_open(const char *btn_str, menuitem_cb_t accept_cb, void *accept_param);

#ifdef __cplusplus
}
#endif

#endif