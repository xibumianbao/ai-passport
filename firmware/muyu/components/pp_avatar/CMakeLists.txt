idf_component_register(
    SRCS "pp_avatar.c" "pp_pixel_art.c" "pp_pixel_buffer.c" "pp_avatar_view.c"
         "../../main/font_pet_14.c" "../../assets/images/pp_pet_room_i4.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
