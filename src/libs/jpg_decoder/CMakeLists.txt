project(jpg_decoder C)

find_package(PkgConfig)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
pkg_check_modules(LIBJPEG REQUIRED libjpeg)

add_library(jpg_decoder STATIC "")

target_compile_options(jpg_decoder PRIVATE)
target_sources(jpg_decoder PRIVATE "${CMAKE_CURRENT_LIST_DIR}/jpg_decoder.c")
target_include_directories(jpg_decoder PUBLIC "${CMAKE_CURRENT_LIST_DIR}/" ${SDL2_INCLUDE_DIRS} ${TURBOJPEG_INCLUDE_DIRS})
target_compile_options(jpg_decoder PUBLIC ${SDL2_CFLAGS_OTHER})
target_link_libraries(jpg_decoder SDL2 ${SDL2_LIBRARIES} ${TURBOJPEG_LIBRARIES} ${LIBJPEG_LIBRARIES})
