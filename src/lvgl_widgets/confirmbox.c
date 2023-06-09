// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "../lithiumx.h"

static const int key_escape = LV_KEY_ESC;

typedef struct confirmbox_cb
{
    lv_obj_t *parent;
    menuitem_cb_t cb;
    void *param;
} confirmbox_cb_t;

static void confirmbox_cancel(void *param)
{
    confirmbox_cb_t *cb = param;
    lv_event_send(cb->parent, LV_EVENT_KEY, (void *)&key_escape);
    lv_mem_free(param);
}

static void confirmbox_accept(void *param)
{
    confirmbox_cb_t *cb = param;
    cb->cb(cb->param);
    lv_event_send(cb->parent, LV_EVENT_KEY, (void *)&key_escape);
    lv_mem_free(cb);
}

lv_obj_t *confirmbox_open(const char *btn_str, menuitem_cb_t accept_cb, void *accept_param)
{
    confirmbox_cb_t *cb = lv_mem_alloc(sizeof(confirmbox_cb_t));
    cb->cb = accept_cb;
    cb->param = accept_param;

    menu_items_t items[] =
    {
        {"Cancel", confirmbox_cancel, cb, NULL},
        {btn_str, confirmbox_accept, cb, NULL},
    };

    cb->parent = menu_open(items, DASH_ARRAY_SIZE(items));
    return cb->parent;
}
