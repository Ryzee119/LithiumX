#include <lithiumx.h>

static bool debug_info_visible = false;
static SDL_Thread *debug_info_thread;

#ifdef NXDK
static void get_ram_usage(uint32_t *mem_size, uint32_t *mem_used)
{
    MM_STATISTICS MemoryStatistics;
    MemoryStatistics.Length = sizeof(MM_STATISTICS);
    MmQueryStatistics(&MemoryStatistics);
    *mem_size = (MemoryStatistics.TotalPhysicalPages * PAGE_SIZE) / 1024U / 1024U;
    *mem_used = *mem_size - ((MemoryStatistics.AvailablePages * PAGE_SIZE) / 1024U / 1024U);
    return;
}
#else
static void get_ram_usage(uint32_t *mem_size, uint32_t *mem_used)
{
    *mem_size = 0;
    *mem_used = 0;
}
#endif

static void frame_count_incrememt(lv_timer_t *t)
{
    uint32_t *frame_count = t->user_data;
    *frame_count = *frame_count + 1;
}

static int debug_info_thread_f(void *param)
{
    lvgl_getlock();
    lv_obj_t *mem_usage_label = lv_label_create(lv_layer_sys());
    lv_obj_set_style_bg_opa(mem_usage_label, LV_OPA_50, 0);
    lv_obj_set_align(mem_usage_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_style_text_font(mem_usage_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lvgl_removelock();

    uint32_t frame_count = 0, used, capacity, fps = 0, start = lv_tick_get(), ram_used, ram_total;
    lv_timer_t *frame_counter = lv_timer_create(frame_count_incrememt, 0, &frame_count);

    while (1)
    {
        if (debug_info_visible == false)
        {
            break;
        }
        if (frame_count > 100)
        {

            fps = 1000 * frame_count / lv_tick_elaps(start);
            if (1000 / fps < LV_DISP_DEF_REFR_PERIOD)
                fps = 1000 / LV_DISP_DEF_REFR_PERIOD;
            start = lv_tick_get();
            frame_count = 0;
        }
        lx_mem_usage(&used, &capacity);
        get_ram_usage(&ram_total, &ram_used);
        lvgl_getlock();
        lv_label_set_text_fmt(mem_usage_label, "GUI:%d/%dkB\n"
                                               "CPU: %d%%\n"
                                               "RAM:%d/%d MB\n"
                                               "FPS: %d",
                              used / 1024, capacity / 1024, 100 - lv_timer_get_idle(), ram_used, ram_total, fps);

        lv_obj_update_layout(mem_usage_label);
        lv_obj_set_size(mem_usage_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lvgl_removelock();
        for (int i = 0; i < 50; i++)
        {
            SDL_Delay(10);
            if (debug_info_visible == false)
                break;
        }
    }

    lv_timer_del(frame_counter);
    lv_obj_del(mem_usage_label);
    return 0;
}

void dash_debug_open()
{
    debug_info_visible = true;
    debug_info_thread = SDL_CreateThread(debug_info_thread_f, "update_mem_usage", NULL);
}

void dash_debug_close()
{
    debug_info_visible = false;
    SDL_WaitThread(debug_info_thread, NULL);
}
