// SPDX-License-Identifier: MIT

#include "../../lv_port_indev.h"
#include "lvgl.h"
#ifdef NXDK
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

static lv_indev_drv_t indev_drv_gamepad;
static lv_indev_drv_t indev_drv_mouse;
static lv_indev_t *indev_mouse;
static lv_indev_t *indev_keypad;
static lv_obj_t *mouse_cursor;
static SDL_GameController *pad = NULL;
static int mouse_x, mouse_y;
static lv_quit_event_t quit_event = LV_QUIT_NONE;
static bool mouse_event = false;
static bool mouse_pressed = false;
#ifndef MOUSE_SENSITIVITY
#define MOUSE_SENSITIVITY 50 // pixels per input poll LV_INDEV_DEF_READ_PERIOD
#endif
#ifndef MOUSE_DEADZONE
#define MOUSE_DEADZONE 10 // Percent
#endif

#ifndef LVGL_USE_CUSTOM_CONTROLLER_MAP
static gamecontroller_map_t lvgl_gamecontroller_map[] =
    {
        {.sdl_map = SDL_CONTROLLER_BUTTON_A, .lvgl_map = LV_KEY_ENTER},
        {.sdl_map = SDL_CONTROLLER_BUTTON_B, .lvgl_map = LV_KEY_ESC},
        {.sdl_map = SDL_CONTROLLER_BUTTON_X, .lvgl_map = LV_KEY_BACKSPACE},
        {.sdl_map = SDL_CONTROLLER_BUTTON_Y, .lvgl_map = LV_KEY_HOME},
        {.sdl_map = SDL_CONTROLLER_BUTTON_BACK, .lvgl_map = LV_KEY_PREV},
        {.sdl_map = SDL_CONTROLLER_BUTTON_GUIDE, .lvgl_map = 0},
        {.sdl_map = SDL_CONTROLLER_BUTTON_START, .lvgl_map = LV_KEY_NEXT},
        {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSTICK, .lvgl_map = LV_KEY_PREV},
        {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSTICK, .lvgl_map = LV_KEY_NEXT},
        {.sdl_map = SDL_CONTROLLER_BUTTON_LEFTSHOULDER, .lvgl_map = LV_KEY_PREV},
        {.sdl_map = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, .lvgl_map = LV_KEY_NEXT},
        {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_UP, .lvgl_map = LV_KEY_UP},
        {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_DOWN, .lvgl_map = LV_KEY_DOWN},
        {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_LEFT, .lvgl_map = LV_KEY_LEFT},
        {.sdl_map = SDL_CONTROLLER_BUTTON_DPAD_RIGHT, .lvgl_map = LV_KEY_RIGHT}};
#else
extern gamecontroller_map_t lvgl_gamecontroller_map[];
#endif

#ifndef LVGL_USE_CUSTOM_KEYBOARD_MAP
static keyboard_map_t lvgl_keyboard_map[] =
    {
        {.sdl_map = SDLK_ESCAPE, .lvgl_map = LV_KEY_ESC},
        {.sdl_map = SDLK_BACKSPACE, .lvgl_map = LV_KEY_BACKSPACE},
        {.sdl_map = SDLK_HOME, .lvgl_map = LV_KEY_HOME},
        {.sdl_map = SDLK_RETURN, .lvgl_map = LV_KEY_ENTER},
        {.sdl_map = SDLK_PAGEDOWN, .lvgl_map = LV_KEY_PREV},
        {.sdl_map = SDLK_PAGEUP, .lvgl_map = LV_KEY_NEXT},
        {.sdl_map = SDLK_TAB, .lvgl_map = LV_KEY_NEXT},
        {.sdl_map = SDLK_UP, .lvgl_map = LV_KEY_UP},
        {.sdl_map = SDLK_DOWN, .lvgl_map = LV_KEY_DOWN},
        {.sdl_map = SDLK_LEFT, .lvgl_map = LV_KEY_LEFT},
        {.sdl_map = SDLK_RIGHT, .lvgl_map = LV_KEY_RIGHT},
        {.sdl_map = 0, .lvgl_map = 0}};
#else
extern keyboard_map_t lvgl_keyboard_map[];
#endif

lv_quit_event_t lv_get_quit(void)
{
    return quit_event;
}

void lv_set_quit(lv_quit_event_t event)
{
    quit_event = event;
}

static void mouse_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void) indev_drv;

    if (pad == NULL)
    {
        return;
    }

    data->state = (mouse_pressed) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    // Event for a USB mouse
    if (mouse_event)
    {
        uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        data->point.x = mouse_x;
        data->point.y = mouse_y;

        unsigned int *_state = (unsigned int *)&data->state;
        *_state |= (buttons & SDL_BUTTON_LMASK) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        mouse_event = false;
    }
    // From gamecontroller
    else
    {
        int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);

        if (SDL_abs(x) > (MOUSE_DEADZONE * 32768) / 100)
        {
            mouse_x += (x * MOUSE_SENSITIVITY) / 32768;
            if (mouse_x < 0)
                mouse_x = 0;
            if (mouse_x > 640)
                mouse_x = 640;
        }

        if (SDL_abs(y) > (MOUSE_DEADZONE * 32768) / 100)
        {
            mouse_y += (y * MOUSE_SENSITIVITY) / 32768;
            if (mouse_y < 0)
                mouse_y = 0;
            if (mouse_y > 640)
                mouse_y = 640;
        }

        data->point.x = (int16_t)mouse_x;
        data->point.y = (int16_t)mouse_y;
    }
}

#define REPEAT_INITIAL_DELAY            600 // Time before the key starts repeating
#define REPEAT_DELAY                     80 // Time between key state repeats

static uint32_t repeat_key         = NULL;  // Key to repeat
static uint32_t repeat_time_last   = 0;     // Time the key was last pressed
static uint32_t repeat_time_first  = 0;     // Time the key was first pressed
static bool     repeat_key_pressed = false; // If the key is currently pressed

static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void) indev_drv;
    data->key = 0;

    // Check if we have data to process
    if(SDL_PollEvent(NULL) == 0) {
        // Simulate repeat key
        if(
            // Check we have a key to repeat
            repeat_key != NULL &&
            // Check if enough time has passed to repeat
            (SDL_GetTicks() - repeat_time_first > REPEAT_INITIAL_DELAY) && (SDL_GetTicks() - repeat_time_last > REPEAT_DELAY)
        ) {
            data->key   = repeat_key;
            data->state = repeat_key_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

            repeat_key_pressed = !repeat_key_pressed;
            repeat_time_last = SDL_GetTicks();
        }

        data->continue_reading = false;
        return;
    }

    static SDL_Event e;
    if (SDL_PollEvent(&e))
    {
        if (e.type == SDL_WINDOWEVENT || e.type == SDL_QUIT)
        {
            if (e.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                quit_event = LV_SHUTDOWN;
            }
        }

        // Handle controller hotplugging
        if (e.type == SDL_CONTROLLERDEVICEADDED)
        {
            SDL_GameController *new_pad = SDL_GameControllerOpen(e.cdevice.which);
            if (pad == NULL)
            {
                pad = new_pad;
            }
        }

        if (e.type == SDL_CONTROLLERDEVICEREMOVED)
        {
            if (pad == SDL_GameControllerFromInstanceID(e.cdevice.which))
            {
                pad = NULL;
            }
            SDL_GameControllerClose(SDL_GameControllerFromInstanceID(e.cdevice.which));
        }

        // Parse some mouse events while we are here.
        if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
        {
            mouse_event = true;
        }

        if ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.button == SDL_BUTTON_LEFT)
        {
            mouse_event = true;
            mouse_pressed = (e.type == SDL_MOUSEBUTTONDOWN);
        }

        if ((e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) && e.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK)
        {
            mouse_pressed = (e.type == SDL_CONTROLLERBUTTONDOWN);
        }

        // Handle controller button events
        if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
        {
            // If we hit this assert, lvgl_gamecontroller_map isnt right
            LV_ASSERT(lvgl_gamecontroller_map[e.cbutton.button].sdl_map == e.cbutton.button);
            data->key = lvgl_gamecontroller_map[e.cbutton.button].lvgl_map;
            data->state = (e.type == SDL_CONTROLLERBUTTONDOWN) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

            // If we hit this assert, lvgl_gamecontroller_map isnt right
            LV_ASSERT(lvgl_gamecontroller_map[e.cbutton.button].sdl_map == e.cbutton.button);
            data->key = lvgl_gamecontroller_map[e.cbutton.button].lvgl_map;
            data->state = (e.type == SDL_CONTROLLERBUTTONDOWN) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

            // Store the last key pressed for repeat
            if (
                data->state == LV_INDEV_STATE_PRESSED && (
                    data->key == LV_KEY_UP   || data->key == LV_KEY_DOWN ||
                    data->key == LV_KEY_LEFT || data->key == LV_KEY_RIGHT
                )
            )
            {
                repeat_key  = data->key;
                repeat_time_last = repeat_time_first = SDL_GetTicks();
            }

            // Clear the repeat key if it was released
            if (data->state == LV_INDEV_STATE_RELEASED && data->key == repeat_key)
                repeat_key = 0;
        }

        if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
            static bool pressed = 0;
            data->key = 'L';
            if (e.caxis.value > 0x20 && pressed == 0)
            {
                data->key = 'L';
                data->state = LV_INDEV_STATE_PRESSED;
                pressed = 1;
            }
            else if (e.caxis.value < 0x10 && pressed == 1)
            {
                data->key = 'L';
                data->state = LV_INDEV_STATE_RELEASED;
                pressed = 0;
            }
        }

        if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            static bool pressed = 0;
            data->key = 'R';
            if (e.caxis.value > 0x20 && pressed == 0)
            {
                data->key = 'R';
                data->state = LV_INDEV_STATE_PRESSED;
                pressed = 1;
            }
            else if (e.caxis.value < 0x10 && pressed == 1)
            {
                data->key = 'R';
                data->state = LV_INDEV_STATE_RELEASED;
                pressed = 0;
            }
        }

        // Handle keyboard button events
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
        {
            int i = 0;
            data->key = 0;
            while (lvgl_keyboard_map[i].sdl_map !=0)
            {
                if (lvgl_keyboard_map[i].sdl_map == e.key.keysym.sym)
                {
                    data->key = lvgl_keyboard_map[i].lvgl_map;
                    data->state = (e.type == SDL_KEYUP) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
                    break;
                }
                i++;
            }
            if (data->key == 0)
            {
                data->key = e.key.keysym.sym;
                data->state = (e.type == SDL_KEYUP) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            }
        }
    }

    // Always report that we are ready to read more data so we can process the repeat key
    // if needed on the next poll.
    data->continue_reading = true;
}

void lv_port_indev_init(bool use_mouse_cursor)
{
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        SDL_GameControllerOpen(i);
    }

    // Register the gamepad as a keypad
    lv_indev_drv_init(&indev_drv_gamepad);
    indev_drv_gamepad.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv_gamepad.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv_gamepad);

    // Register a mouse cursor
    if (use_mouse_cursor)
    {
        lv_indev_drv_init(&indev_drv_mouse);
        indev_drv_mouse.type = LV_INDEV_TYPE_POINTER;
        indev_drv_mouse.read_cb = mouse_read;
        indev_mouse = lv_indev_drv_register(&indev_drv_mouse);
        mouse_cursor = lv_img_create(lv_scr_act());
        lv_img_set_src(mouse_cursor, LV_SYMBOL_PLUS);
        lv_indev_set_cursor(indev_mouse, mouse_cursor);
    }
    quit_event = LV_QUIT_NONE;
}

void lv_port_indev_deinit(void)
{
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}
