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

static lv_timer_t *frame_counter_timer;
static lv_timer_t *debug_info_timer;
static lv_obj_t *debug_info_label;
static uint32_t frame_counter;

static void frame_count_incrememt(lv_timer_t *t)
{
    frame_counter++;
}

static void debug_info_callback(lv_timer_t *timer)
{
    uint32_t used, capacity, ram_used, ram_total;

    uint32_t fps = frame_counter * 1000 / timer->period;
    frame_counter = 0;

    lx_mem_usage(&used, &capacity);
    get_ram_usage(&ram_total, &ram_used);
    lv_label_set_text_fmt(debug_info_label, "GUI:%d/%dkB\n"
                                           "CPU: %d%%\n"
                                           "RAM:%d/%d MB\n"
                                           "FPS: %d",
                          used / 1024, capacity / 1024, 100 - lv_timer_get_idle(), ram_used, ram_total, fps);

    lv_obj_update_layout(debug_info_label);
    lv_obj_set_size(debug_info_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
}

void dash_debug_open()
{
    frame_counter = 0;
    frame_counter_timer = lv_timer_create(frame_count_incrememt, 0, NULL);
    debug_info_label = lv_label_create(lv_layer_sys());

    lv_obj_set_style_bg_opa(debug_info_label, LV_OPA_50, 0);
    lv_obj_set_align(debug_info_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_style_text_font(debug_info_label, &lv_font_montserrat_16, LV_PART_MAIN);

    debug_info_timer = lv_timer_create(debug_info_callback, 500, NULL);
    lv_timer_ready(debug_info_timer);
}

void dash_debug_close()
{
    lv_timer_del(frame_counter_timer);
    lv_timer_del(debug_info_timer);
    lv_obj_del(debug_info_label);
    debug_info_label = NULL;
    frame_counter_timer = NULL;
}
