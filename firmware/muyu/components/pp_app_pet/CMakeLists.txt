idf_component_register(
    SRCS "pp_pet.c" "pp_pet_app.c" "pp_pet_store.c" "pp_pet_ui.c" "../../main/font_pet_14.c"
         "../../assets/images/pp_pet_room_i4.c"
    INCLUDE_DIRS "include" "../../main"
    REQUIRES bsp nvs_flash esp_hw_support pp_avatar
)
