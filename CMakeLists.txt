cmake_minimum_required(VERSION 3.13)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(LithiumX C)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize-undefined-trap-on-error -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG")

find_package(SDL2 REQUIRED)
find_package(JPEG REQUIRED)

add_library(sqlite)
target_compile_options(sqlite PRIVATE)
target_compile_definitions(sqlite PRIVATE
    -DSQLITE_OS_OTHER=1 #To match xbox environment, set no OS then pull in my own OS driver
    -DSQLITE_DEFAULT_MEMSTATUS=0
    -DSQLITE_OMIT_DEPRECATED
    -DSQLITE_OMIT_PROGRESS_CALLBACK
    -DSQLITE_OMIT_SHARED_CACHE
    -DSQLITE_OMIT_AUTOINIT
    -DSQLITE_DISABLE_INTRINSIC
    -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    )
target_include_directories(sqlite PRIVATE src/libs)
target_sources(sqlite PRIVATE "src/libs/sqlite3/sqlite3.c" "src/platform/win32/sqlite_win32.c")

add_library(toml)
target_sources(toml PRIVATE "src/libs/toml/toml.c")

add_library(sxml)
target_sources(sxml PRIVATE "src/libs/sxml/sxml.c")

add_library(jpg_decoder)
target_sources(jpg_decoder PRIVATE "src/libs/jpg_decoder/jpg_decoder.c")
target_link_libraries(jpg_decoder PRIVATE JPEG::JPEG SDL2::SDL2)

#lvgl lib
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES 1)
set(LV_CONF_BUILD_DISABLE_DEMOS 1)
include(src/libs/lvgl/CMakeLists.txt)

set(SOURCES
    src/main.c
    src/dash_database.c
    src/dash_main.c
    src/dash_scroller.c
    src/dash_styles.c
    src/dash_synop.c
    src/dash_mainmenu.c
    src/dash_settings.c
    src/dash_eeprom.c
    src/dash_browser.c
    src/lvgl_widgets/confirmbox.c
    src/lvgl_widgets/menu.c
    src/lvgl_widgets/generic_container.c
)
if(NXDK)
    list(APPEND SOURCES src/lvgl_drivers/input/sdl/lv_sdl_indev.c)
    list(APPEND SOURCES src/platform/xbox/platform.c)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/lv_xgu_disp.c)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/lv_xgu_draw.c)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/lv_xgu_rect.c)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/lv_xgu_texture.c)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/notexture.ps)
    list(APPEND SOURCES src/lvgl_drivers/video/xgu/texture.ps)
else()
    list(APPEND SOURCES src/lvgl_drivers/input/sdl/lv_sdl_indev.c)
    list(APPEND SOURCES src/lvgl_drivers/video/sdl/lv_sdl_disp.c)
    list(APPEND SOURCES src/platform/win32/platform.c)
endif()
add_executable(LithiumX ${SOURCES})

target_compile_options(LithiumX PUBLIC -Wall -Wextra -std=c99)
target_compile_definitions(LithiumX PUBLIC "-DLVGL_USE_CUSTOM_KEYBOARD_MAP")
target_include_directories(LithiumX PUBLIC . libs)

if(TARGET SDL2::SDL2main)
    target_link_libraries(LithiumX PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(LithiumX PRIVATE lvgl sqlite jpg_decoder toml sxml SDL2::SDL2)

#target_compile_options(LithiumX PRIVATE -O2)