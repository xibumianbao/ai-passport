cmake_minimum_required(VERSION 3.16)
project(muyu_preview C)
set(CMAKE_C_STANDARD 11)
get_filename_component(FIRMWARE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LVGL_SOURCE_DIR "${FIRMWARE_ROOT}/managed_components/lvgl__lvgl" CACHE PATH "Pinned LVGL sources")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" lvgl)
add_executable(muyu_preview preview.c
    "${FIRMWARE_ROOT}/main/muyu_ui.c"
    "${FIRMWARE_ROOT}/main/ui_pixel.c"
    "${FIRMWARE_ROOT}/main/font_muyu_22.c")
target_include_directories(muyu_preview PRIVATE "${FIRMWARE_ROOT}/main")
target_compile_options(muyu_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(muyu_preview PRIVATE lvgl::lvgl m)
add_executable(muyu_audio "${FIRMWARE_ROOT}/tests/test_muyu_logic.c"
    "${FIRMWARE_ROOT}/main/muyu_logic.c")
target_include_directories(muyu_audio PRIVATE "${FIRMWARE_ROOT}/main")
target_link_libraries(muyu_audio PRIVATE m)
add_executable(passport_preview passport_preview.c
    "${FIRMWARE_ROOT}/main/passport_ui.c" "${FIRMWARE_ROOT}/main/font_muyu_22.c")
target_include_directories(passport_preview PRIVATE "${FIRMWARE_ROOT}/main")
target_compile_options(passport_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(passport_preview PRIVATE lvgl::lvgl m)
