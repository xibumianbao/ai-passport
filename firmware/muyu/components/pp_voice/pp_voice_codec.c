#include "pp_voice_codec.h"
#include "pp_voice_wire.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include <stddef.h>

void pp_voice_codec_close(pp_voice_codec_t *codec)
{
    if(codec->handle) {
        if(codec->mode==PP_CODEC_ENCODE) esp_opus_enc_close(codec->handle);
        else if(codec->mode==PP_CODEC_DECODE) esp_opus_dec_close(codec->handle);
    }
    codec->handle=NULL; codec->mode=PP_CODEC_OFF;
}
esp_audio_err_t pp_voice_codec_select(pp_voice_codec_t *codec,pp_codec_mode_t mode)
{
    if(mode!=PP_CODEC_OFF && mode!=PP_CODEC_ENCODE && mode!=PP_CODEC_DECODE)
        return ESP_AUDIO_ERR_INVALID_PARAMETER;
    if(codec->mode==mode && (mode==PP_CODEC_OFF || codec->handle)) return ESP_AUDIO_ERR_OK;
    pp_voice_codec_close(codec);
    if(mode==PP_CODEC_OFF) return ESP_AUDIO_ERR_OK;
    codec->mode=mode;
    esp_audio_err_t error;
    if(mode==PP_CODEC_ENCODE) {
        esp_opus_enc_config_t cfg={.sample_rate=16000,.channel=1,.bits_per_sample=16,.bitrate=24000,
            .frame_duration=ESP_OPUS_ENC_FRAME_DURATION_60_MS,.application_mode=ESP_OPUS_ENC_APPLICATION_VOIP,
            .complexity=0,.enable_fec=false,.enable_dtx=false,.enable_vbr=true};
        error=esp_opus_enc_open(&cfg,sizeof(cfg),&codec->handle);
        if(error==ESP_AUDIO_ERR_OK && codec->handle) {
            int input=0,output=0;
            error=esp_opus_enc_get_frame_size(codec->handle,&input,&output);
            if(error==ESP_AUDIO_ERR_OK &&
               (input!=PP_VOICE_PCM_BYTES || output<=0 || output>PP_VOICE_OPUS_MAX))
                error=ESP_AUDIO_ERR_INVALID_PARAMETER;
        }
    } else {
        esp_opus_dec_cfg_t cfg={.sample_rate=16000,.channel=1,
            .frame_duration=ESP_OPUS_DEC_FRAME_DURATION_60_MS,.self_delimited=false};
        error=esp_opus_dec_open(&cfg,sizeof(cfg),&codec->handle);
    }
    if(error==ESP_AUDIO_ERR_OK && !codec->handle) error=ESP_AUDIO_ERR_FAIL;
    if(error!=ESP_AUDIO_ERR_OK) pp_voice_codec_close(codec);
    return error;
}
