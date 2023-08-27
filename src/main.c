#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_SNPRINTF_SAFE_TRIM_STRING_ON_OVERFLOW
#include <lvgl.h>
#include "lithiumx.h"

static CRITICAL_SECTION tlsf_crit_sec;
static tlsf_t mem_pool;
static uint8_t mem_pool_data[3U * 1024U * 1024U];

static SDL_mutex *lvgl_mutex;

keyboard_map_t lvgl_keyboard_map[] =
{
    {.sdl_map = SDLK_ESCAPE, .lvgl_map = DASH_SETTINGS_PAGE},
    {.sdl_map = SDLK_BACKSPACE, .lvgl_map = LV_KEY_ESC},
    {.sdl_map = SDLK_RETURN, .lvgl_map = LV_KEY_ENTER},
    {.sdl_map = SDLK_PAGEDOWN, .lvgl_map = DASH_PREV_PAGE},
    {.sdl_map = SDLK_PAGEUP, .lvgl_map = DASH_NEXT_PAGE},
    {.sdl_map = SDLK_UP, .lvgl_map = LV_KEY_UP},
    {.sdl_map = SDLK_DOWN, .lvgl_map = LV_KEY_DOWN},
    {.sdl_map = SDLK_LEFT, .lvgl_map = LV_KEY_LEFT},
    {.sdl_map = SDLK_RIGHT, .lvgl_map = LV_KEY_RIGHT},
    {.sdl_map = 0, .lvgl_map = 0}
};

gamecontroller_map_t lvgl_gamecontroller_map[] =
{
    {.sdl_map = SDL_CONTROLLER_BUTTON_A, .lvgl_map = LV_KEY_ENTER},
    {.sdl_map = SDL_CONTROLLER_BUTTON_B, .lvgl_map = LV_KEY_ESC},
    {.sdl_map = SDL_CONTROLLER_BUTTON_X, .lvgl_map = LV_KEY_BACKSPACE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_Y, .lvgl_map = DASH_INFO_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_BACK, .lvgl_map = DASH_INFO_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_GUIDE, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_START, .lvgl_map = DASH_SETTINGS_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSTICK, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSTICK, .lvgl_map = 0},
    {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, .lvgl_map = DASH_PREV_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, .lvgl_map = DASH_NEXT_PAGE},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_UP, .lvgl_map = LV_KEY_UP},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_DOWN, .lvgl_map = LV_KEY_DOWN},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_LEFT, .lvgl_map = LV_KEY_LEFT},
    {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_RIGHT, .lvgl_map = LV_KEY_RIGHT},
    {.sdl_map = 0, .lvgl_map = 0}
};

// lvgl isn't thread safe, but we can somewhat make it
// by wrapping task handler and any other interactions with these locks
void lvgl_getlock(void)
{
    if (SDL_LockMutex(lvgl_mutex))
    {
        assert(0);
    }
}

void lvgl_removelock(void)
{
    if (SDL_UnlockMutex(lvgl_mutex))
    {
        assert(0);
    }
}

// Output handler for lvgl
void lvgl_putstring(const char *buf)
{
    #ifdef NXDK
    DbgPrint("%s", buf);
    #else
    printf("%s", buf);
    #endif
}

// Replace lvgls internal allocator with basically the same thing
// but wrapped in crit sec for thread safety.
void *lx_mem_alloc(size_t size)
{
    EnterCriticalSection(&tlsf_crit_sec);
    void *ptr = tlsf_malloc(mem_pool, size);
    LeaveCriticalSection(&tlsf_crit_sec);
    return ptr;
}

void *lx_mem_realloc(void *data, size_t new_size)
{
    EnterCriticalSection(&tlsf_crit_sec);
    void *ptr = tlsf_realloc(mem_pool, data, new_size);
    LeaveCriticalSection(&tlsf_crit_sec);
    return ptr;
}

void lx_mem_free(void *data)
{
    EnterCriticalSection(&tlsf_crit_sec);
    tlsf_free(mem_pool, data);
    LeaveCriticalSection(&tlsf_crit_sec);
}

static void npf_putchar(int c, void *ctx)
{
    (void)ctx;
    #ifdef NXDK
    DbgPrint("%c", c);
    #else
    printf("%c", c);
    #endif
}

void dash_printf(dash_debug_level_t level, const char *format, ...)
{
    if (level < NANO_DEBUG_LEVEL)
    {
        return;
    }
    va_list argList;
    va_start(argList, format);
    npf_vpprintf(npf_putchar, NULL, format, argList);
    va_end(argList);
}

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int w,h;
    InitializeCriticalSection(&tlsf_crit_sec);
    mem_pool = tlsf_create_with_pool(mem_pool_data, sizeof(mem_pool_data));

    dash_printf(LEVEL_TRACE, "Initialising Platform\n");
    platform_init(&w, &h);

    lvgl_mutex = SDL_CreateMutex();
    assert(lvgl_mutex);

    dash_printf(LEVEL_TRACE, "Initialising LVGL\n");
    lv_init();
    lv_log_register_print_cb(lvgl_putstring);
    dash_printf(LEVEL_TRACE, "Initialising Display at w: %d, h: %d\n", w, h);
    lv_port_disp_init(w, h);
    lv_port_indev_init(false);

    dash_printf(LEVEL_TRACE, "Creating dash\n");
    dash_init();
    dash_printf(LEVEL_TRACE, "Enter dash busy loop\n");
    while (lv_get_quit() == LV_QUIT_NONE)
    {
        int s,e,t;
        s = SDL_GetTicks();
        lvgl_getlock();
        lv_task_handler();
        lvgl_removelock();
        e = SDL_GetTicks();
        t = e - s;
        if (t < LV_DISP_DEF_REFR_PERIOD)
        {
            SDL_Delay(LV_DISP_DEF_REFR_PERIOD - t);
        }
    }
    dash_printf(LEVEL_TRACE, "Quitting dash with quit event %d\n", lv_get_quit());
    lv_port_disp_deinit();
    lv_port_indev_deinit();
    platform_quit(lv_get_quit());
    return 0;
}
