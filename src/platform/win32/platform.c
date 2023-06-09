#include <windows.h>
#include <lvgl.h>
#include "lithiumx.h"
#include "../platform.h"
#include "lvgl_drivers/lv_port_disp.h"
#include "lvgl_drivers/lv_port_indev.h"

void platform_init(int *w, int *h)
{
    *w = 640;
    *h = 480;
    printf("%s\n", __FUNCTION__);
}

void platform_quit(lv_quit_event_t event)
{
    if (event == LV_REBOOT)
    {
        printf("LV_REBOOT\n");
    }
    else if (event == LV_SHUTDOWN)
    {
        printf("SHUTDOWN\n");
    }
    else if (event == LV_QUIT_OTHER)
    {
        const char *launch_path = dash_launch_path;
        printf("launch exe %s\n", launch_path);
    }
}

void platform_system_info(lv_obj_t *window)
{
    lv_obj_t *label = lv_label_create(window);
    lv_label_set_text(label, "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\n");

}

void platform_flush_cache()
{

}

void platform_get_iso8601_time(char time_str[20])
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    lv_snprintf(time_str, 20, "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
