idf_component_register(
    SRCS "passport_app.c"
         "passport_core.c"
         "passport_ui.c"
         "passport_audio.c"
         "passport_radio.c"
         "passport_ble.c"
         "muyu_ui.c"
         "muyu_logic.c"
         "font_muyu_22.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash esp_wifi esp_event esp_netif esp_http_server bt esp_app_format
)
