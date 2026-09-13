idf_component_register(
    SRCS "muyu_app.c"
         "muyu_ui.c"
         "muyu_logic.c"
         "font_muyu_22.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)
