cmake_minimum_required(VERSION 3.13)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(LithiumX C)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize-undefined-trap-on-error -fno-omit-frame-pointer")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG")

find_package(PkgConfig)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
pkg_check_modules(LIBJPEG REQUIRED libjpeg)

add_library(sqlite)
set(SQLITE_COMPILE_DEFINITIONS)
list(APPEND SQLITE_COMPILE_DEFINITIONS
    -DSQLITE_DEFAULT_MEMSTATUS=0
    -DSQLITE_OMIT_DEPRECATED
    -DSQLITE_OMIT_PROGRESS_CALLBACK
    -DSQLITE_OMIT_SHARED_CACHE
    -DSQLITE_OMIT_AUTOINIT
    -DSQLITE_DISABLE_INTRINSIC
    -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    )
target_compile_options(sqlite PRIVATE)
target_include_directories(sqlite PRIVATE src/libs)
if(WIN32)
list(APPEND SQLITE_COMPILE_DEFINITIONS -DSQLITE_OS_OTHER)
target_sources(sqlite PRIVATE "src/libs/sqlite3/sqlite3.c" "src/platform/win32/sqlite_win32.c")
elseif(UNIX)
target_sources(sqlite PRIVATE "src/libs/sqlite3/sqlite3.c")
endif()
target_compile_definitions(sqlite PRIVATE ${SQLITE_COMPILE_DEFINITIONS})

add_library(toml)
target_sources(toml PRIVATE "src/libs/toml/toml.c")

add_library(sxml)
target_sources(sxml PRIVATE "src/libs/sxml/sxml.c")

add_library(tlsf)
target_sources(tlsf PRIVATE "src/libs/tlsf/tlsf.c")

add_library(jpg_decoder)
target_sources(jpg_decoder PRIVATE "src/libs/jpg_decoder/jpg_decoder.c")
target_include_directories(jpg_decoder PUBLIC ${SDL2_INCLUDE_DIRS} ${TURBOJPEG_INCLUDE_DIRS})
target_compile_options(jpg_decoder PUBLIC ${SDL2_CFLAGS_OTHER})
target_link_libraries(jpg_decoder PRIVATE ${SDL2_LIBRARIES} ${TURBOJPEG_LIBRARIES} ${LIBJPEG_LIBRARIES})

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
    src/dash_debug.c
    src/dash_launcher.c
    src/lvgl_widgets/confirmbox.c
    src/lvgl_widgets/menu.c
    src/lvgl_widgets/generic_container.c
    src/lvgl_widgets/helpers.c
)
if(UNIX)
    list(APPEND SOURCES src/lvgl_drivers/input/sdl/lv_sdl_indev.c)
    list(APPEND SOURCES src/lvgl_drivers/video/sdl/lv_sdl_disp.c)
    list(APPEND SOURCES src/platform/linux/glue.c)
    list(APPEND SOURCES src/platform/linux/platform.c)
elseif(WIN32)
    list(APPEND SOURCES src/lvgl_drivers/input/sdl/lv_sdl_indev.c)
    list(APPEND SOURCES src/lvgl_drivers/video/sdl/lv_sdl_disp.c)
    list(APPEND SOURCES src/platform/win32/platform.c)
endif()
add_executable(LithiumX ${SOURCES})

target_compile_options(LithiumX PUBLIC -Wall -Wextra)
target_compile_definitions(LithiumX PUBLIC "-DLVGL_USE_CUSTOM_KEYBOARD_MAP")
target_include_directories(LithiumX PUBLIC . libs)

target_link_libraries(LithiumX PRIVATE lvgl sqlite jpg_decoder toml sxml tlsf ${SDL2_LIBRARIES})

#target_compile_options(LithiumX PRIVATE -O2)