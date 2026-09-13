set(PP_GENERATED "${CMAKE_BINARY_DIR}/passport_generated")
execute_process(COMMAND ${PYTHON} "${CMAKE_CURRENT_LIST_DIR}/../tools/generate_catalog.py" "${PP_GENERATED}"
                COMMAND_ERROR_IS_FATAL ANY)
include("${PP_GENERATED}/passport_catalog.cmake")
idf_component_register(
    SRCS "passport_app.c" "passport_core.c" "passport_ui.c" "passport_audio.c" "muyu_logic.c"
         "passport_radio.c" "passport_ble.c" "passport_keyboard.c" "passport_screen.c"
         "passport_storage.c" "${PP_GENERATED}/passport_catalog.c"
    INCLUDE_DIRS "." "${PP_GENERATED}"
    REQUIRES bsp esp_timer nvs_flash esp_wifi esp_event esp_netif bt esp_app_format
             esp_partition spi_flash ${PP_COMPONENTS}
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../apps/catalog.json")
