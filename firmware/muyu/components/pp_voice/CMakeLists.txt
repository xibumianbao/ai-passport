idf_component_register(SRCS "pp_voice.c" "pp_voice_wire.c" "pp_voice_codec.c" "pp_voice_selftest.c"
    INCLUDE_DIRS "include" "../../main"
    REQUIRES bsp nvs_flash esp_http_client tcp_transport esp-tls esp_netif
             esp_timer esp_hw_support json esp_audio_codec)
