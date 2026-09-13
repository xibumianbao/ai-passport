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
    "${FIRMWARE_ROOT}/main/passport_ui.c" "${FIRMWARE_ROOT}/main/font_muyu_22.c"
    "${FIRMWARE_ROOT}/main/passport_core.c" "${FIRMWARE_ROOT}/main/passport_keyboard.c"
    "${FIRMWARE_ROOT}/main/muyu_logic.c" "${FIRMWARE_ROOT}/components/pp_app_muyu/pp_muyu.c")
target_include_directories(passport_preview PRIVATE "${FIRMWARE_ROOT}/main")
target_compile_options(passport_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(passport_preview PRIVATE lvgl::lvgl m)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/capacity_preview.py"
                "${FIRMWARE_ROOT}/build/capacity-report.json" "${CMAKE_BINARY_DIR}/capacity_preview.h"
                COMMAND_ERROR_IS_FATAL ANY)
target_include_directories(passport_preview PRIVATE "${CMAKE_BINARY_DIR}")
set(PP_PROBE "${CMAKE_BINARY_DIR}/catalog_probe")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/catalog_probe.py" "${PP_PROBE}"
                COMMAND_ERROR_IS_FATAL ANY)
add_executable(passport_catalog_test catalog_test.c
    "${PP_PROBE}/generated/passport_catalog.c" "${PP_PROBE}/components/pp_app_probe/pp_app_probe.c"
    "${FIRMWARE_ROOT}/main/passport_ui.c" "${FIRMWARE_ROOT}/main/passport_core.c"
    "${FIRMWARE_ROOT}/main/passport_keyboard.c" "${FIRMWARE_ROOT}/main/muyu_logic.c"
    "${FIRMWARE_ROOT}/main/font_muyu_22.c" "${FIRMWARE_ROOT}/components/pp_app_muyu/pp_muyu.c")
target_include_directories(passport_catalog_test PRIVATE "${FIRMWARE_ROOT}/main" "${PP_PROBE}/generated")
target_compile_options(passport_catalog_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(passport_catalog_test PRIVATE lvgl::lvgl m)
set(PET_ROOT "${FIRMWARE_ROOT}/components/pp_app_pet")
set(AVATAR_ROOT "${FIRMWARE_ROOT}/components/pp_avatar")
add_executable(pet_preview pet_preview.c
    "${PET_ROOT}/pp_pet.c" "${PET_ROOT}/pp_pet_app.c" "${PET_ROOT}/pp_pet_ui.c"
    "${AVATAR_ROOT}/pp_avatar.c" "${AVATAR_ROOT}/pp_pixel_art.c" "${AVATAR_ROOT}/pp_pixel_buffer.c"
    "${FIRMWARE_ROOT}/assets/images/pp_pet_room_i4.c" "${FIRMWARE_ROOT}/main/font_pet_14.c"
    "${FIRMWARE_ROOT}/main/passport_ui.c" "${FIRMWARE_ROOT}/main/passport_core.c"
    "${FIRMWARE_ROOT}/main/passport_keyboard.c" "${FIRMWARE_ROOT}/main/muyu_logic.c"
    "${FIRMWARE_ROOT}/main/font_muyu_22.c" "${FIRMWARE_ROOT}/components/pp_app_muyu/pp_muyu.c")
target_include_directories(pet_preview PRIVATE "${FIRMWARE_ROOT}/main" "${PET_ROOT}/include" "${AVATAR_ROOT}/include")
target_compile_options(pet_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(pet_preview PRIVATE lvgl::lvgl m)
add_executable(xiaozhi_preview xiaozhi_preview.c
    "${FIRMWARE_ROOT}/components/pp_app_xiaozhi/pp_xiaozhi_app.c"
    "${FIRMWARE_ROOT}/components/pp_app_xiaozhi/pp_xiaozhi_ui.c"
    "${PET_ROOT}/pp_pet.c" "${PET_ROOT}/pp_pet_app.c" "${PET_ROOT}/pp_pet_ui.c"
    "${AVATAR_ROOT}/pp_avatar.c" "${AVATAR_ROOT}/pp_pixel_art.c" "${AVATAR_ROOT}/pp_pixel_buffer.c"
    "${FIRMWARE_ROOT}/assets/images/pp_pet_room_i4.c" "${FIRMWARE_ROOT}/main/font_pet_14.c"
    "${FIRMWARE_ROOT}/main/passport_ui.c" "${FIRMWARE_ROOT}/main/passport_core.c"
    "${FIRMWARE_ROOT}/main/passport_keyboard.c")
target_include_directories(xiaozhi_preview PRIVATE "${FIRMWARE_ROOT}/main"
    "${FIRMWARE_ROOT}/components/pp_voice/include" "${PET_ROOT}/include" "${AVATAR_ROOT}/include")
target_compile_options(xiaozhi_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(xiaozhi_preview PRIVATE lvgl::lvgl m)
