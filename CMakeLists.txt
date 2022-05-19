cmake_minimum_required(VERSION 3.14)

project(LithiumX LANGUAGES C)

execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DBUILD_VERSION=\"${GIT_COMMIT_HASH}\"")

find_package(PkgConfig)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(PROJECT_DIR "${CMAKE_SOURCE_DIR}/src")
set(LVGL_DIR "${PROJECT_DIR}/lib/lvgl")
set(LV_CONF_PATH ${PROJECT_DIR}/lv_conf.h CACHE STRING "" FORCE)

add_definitions(-DLVGL_USE_CUSTOM_CONTROLLER_MAP)
add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)
add_definitions(-DLV_CONF_PATH=${LV_CONF_PATH})
add_definitions(-DJPEG_DECODER_QUEUE_SIZE=1024)
add_definitions(-DQRURL=\"https://github.com/Ryzee119/LithiumX\")
#add_definitions(-DDBG) #Show fps and memory usage info

add_compile_options(-Wall -O3)
file(GLOB PROJECT_FILES
    "${PROJECT_DIR}/dash.c"
    "${PROJECT_DIR}/dash_styles.c" 
    "${PROJECT_DIR}/dash_menu.c"
    "${PROJECT_DIR}/dash_synop.c"
    "${PROJECT_DIR}/dash_titlelist.c" 
    "${PROJECT_DIR}/main.c"
    "${PROJECT_DIR}/helpers/nano_debug.c"
    "${PROJECT_DIR}/helpers/fileio.c"
    "${PROJECT_DIR}/platform/generic/platform.c"
    "${PROJECT_DIR}/platform/generic/network.c"
    "${PROJECT_DIR}/dash_resources/default_tbn.c"
    "${PROJECT_DIR}/dash_resources/qrcode.c"
    "${PROJECT_DIR}/lvgl_drivers/lv_sdl_indev.c"
    "${PROJECT_DIR}/lvgl_drivers/sdl/lv_sdl_disp.c"
)
add_executable(LithiumX ${PROJECT_FILES})

include_directories(${LVGL_DIR})
include_directories(${PROJECT_DIR})
include_directories(${PROJECT_DIR}/lib)
include_directories(${PROJECT_DIR}/lvgl_drivers)
include_directories(${LIBJPEG_TURBO_DIR})

add_subdirectory(${LVGL_DIR})
add_subdirectory(${PROJECT_DIR}/lib/jpg_decoder/)
add_subdirectory(${PROJECT_DIR}/lib/xml/)

target_include_directories(LithiumX PUBLIC ${SDL2_INCLUDE_DIRS} ${TURBOJPEG_INCLUDE_DIRS})
target_compile_options(LithiumX PUBLIC ${SDL2_CFLAGS_OTHER})
target_link_libraries(LithiumX ${SDL2_LIBRARIES} ${TURBOJPEG_LIBRARIES}
                           ${LIBJPEG_LIBRARIES} lvgl::lvgl xml jpg_decoder SDL2)
