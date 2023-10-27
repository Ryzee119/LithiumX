// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Ryzee119

#include "../lithiumx.h"

static void input_forwarder(lv_event_t *event)
{
    lv_obj_t *window = lv_event_get_target(event);
    static int key;

    key = *((int *)lv_event_get_param(event));
    lv_obj_t *item = lv_obj_get_child(window, 0);
    assert(lv_obj_is_valid(item));
    if (item)
    {
        lv_event_send(item, LV_EVENT_KEY, &key);
    }
}

static void container_scroll(lv_event_t *event)
{
    lv_obj_t *window = lv_event_get_target(event);
    lv_key_t key = *((lv_key_t *)lv_event_get_param(event));
    if (key == LV_KEY_UP)
    {
        lv_obj_scroll_to_y(window, lv_obj_get_scroll_y(window) - lv_obj_get_height(window) / 4, LV_ANIM_ON);
    }
    else if (key == LV_KEY_DOWN)
    {
        lv_obj_scroll_to_y(window, lv_obj_get_scroll_y(window) + lv_obj_get_height(window) / 4, LV_ANIM_ON);
    }
}

lv_obj_t *container_open()
{
    lv_obj_t *obj = menu_open_static(NULL, 0);
    lv_obj_add_event_cb(obj, input_forwarder, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(obj, container_scroll, LV_EVENT_KEY, NULL);
    lv_obj_update_layout(obj);
    return obj;
}
