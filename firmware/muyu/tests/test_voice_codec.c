/* Exercise the production codec owner with bounded, failing SDK allocators.
 * Byte budgets are synthetic, NOT measurements of the proprietary codec. */
#include "pp_voice_codec.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned kind,bytes; } allocation_t;
static unsigned used,peak,opens,closes;
static esp_audio_err_t open_error,frame_error;
static bool null_success,partial_failure;
static int input_bytes=1920,output_bytes=1275;
static esp_audio_err_t allocate(unsigned kind,void **handle)
{
    unsigned bytes=kind==1?40000:28000;
    *handle=NULL;
    if(null_success) return ESP_AUDIO_ERR_OK;
    if(open_error && !partial_failure) return open_error;
    if(used+bytes>48000) return ESP_AUDIO_ERR_MEM_LACK;
    allocation_t *a=malloc(bytes); assert(a); *a=(allocation_t){kind,bytes};
    used+=bytes; if(used>peak) peak=used;
    ++opens; *handle=a;
    return open_error;
}
static void release(void *handle,unsigned kind)
{
    allocation_t *a=handle; assert(a && a->kind==kind && used>=a->bytes);
    used-=a->bytes; ++closes; free(a);
}
esp_audio_err_t esp_opus_enc_open(void *config,uint32_t size,void **handle)
{
    esp_opus_enc_config_t *c=config;
    assert(size==sizeof(*c) && c->sample_rate==16000 && c->channel==1 && c->bits_per_sample==16);
    assert(c->frame_duration==ESP_OPUS_ENC_FRAME_DURATION_60_MS && c->complexity==0);
    return allocate(1,handle);
}
esp_audio_err_t esp_opus_dec_open(void *config,uint32_t size,void **handle)
{
    esp_opus_dec_cfg_t *c=config;
    assert(size==sizeof(*c) && c->sample_rate==16000 && c->channel==1 && !c->self_delimited);
    return allocate(2,handle);
}
void esp_opus_enc_close(void *handle) { release(handle,1); }
void esp_opus_dec_close(void *handle) { release(handle,2); }
esp_audio_err_t esp_opus_enc_get_frame_size(void *handle,int *input,int *output)
{
    assert(handle && ((allocation_t *)handle)->kind==1);
    *input=input_bytes; *output=output_bytes; return frame_error;
}
static void off(const pp_voice_codec_t *c)
{ assert(!c->handle && c->mode==PP_CODEC_OFF && !used && opens==closes); }
int main(void)
{
    pp_voice_codec_t c={0};
    for(unsigned i=0;i<10000;++i) {
        assert(pp_voice_codec_select(&c,PP_CODEC_ENCODE)==ESP_AUDIO_ERR_OK);
        void *encoder=c.handle; unsigned before=opens;
        assert(pp_voice_codec_select(&c,PP_CODEC_ENCODE)==ESP_AUDIO_ERR_OK);
        assert(c.handle==encoder && opens==before); /* preserve stream state */
        assert(pp_voice_codec_select(&c,PP_CODEC_DECODE)==ESP_AUDIO_ERR_OK);
        assert(used==28000); /* old encoder was freed before decoder open */
        pp_voice_codec_close(&c); pp_voice_codec_close(&c); off(&c);
    }
    assert(peak==40000);
    /* Failure on either direction frees the previous owner, including a
     * defensive partial allocation returned with an SDK error. */
    const esp_audio_err_t errors[]={ESP_AUDIO_ERR_MEM_LACK,ESP_AUDIO_ERR_INVALID_PARAMETER,ESP_AUDIO_ERR_FAIL};
    for(unsigned direction=1;direction<=2;++direction) {
        for(unsigned i=0;i<sizeof(errors)/sizeof(errors[0]);++i) {
            for(unsigned partial=0;partial<=1;++partial) {
                assert(pp_voice_codec_select(&c,direction==1?PP_CODEC_DECODE:PP_CODEC_ENCODE)==0);
                open_error=errors[i]; partial_failure=partial;
                assert(pp_voice_codec_select(&c,(pp_codec_mode_t)direction)==errors[i]); off(&c);
                open_error=0; partial_failure=false;
            }
        }
        null_success=true;
        assert(pp_voice_codec_select(&c,(pp_codec_mode_t)direction)==ESP_AUDIO_ERR_FAIL); off(&c);
        null_success=false;
    }
    frame_error=ESP_AUDIO_ERR_FAIL;
    assert(pp_voice_codec_select(&c,PP_CODEC_ENCODE)==frame_error); off(&c); frame_error=0;
    const int sizes[][2]={{0,1275},{1918,1275},{1920,0},{1920,-1},{1920,1276}};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        input_bytes=sizes[i][0]; output_bytes=sizes[i][1];
        assert(pp_voice_codec_select(&c,PP_CODEC_ENCODE)==ESP_AUDIO_ERR_INVALID_PARAMETER); off(&c);
    }
    input_bytes=1920; output_bytes=1275;
    assert(pp_voice_codec_select(&c,PP_CODEC_ENCODE)==0);
    assert(pp_voice_codec_select(&c,(pp_codec_mode_t)99)==ESP_AUDIO_ERR_INVALID_PARAMETER);
    assert(c.mode==PP_CODEC_ENCODE && c.handle);
    assert(pp_voice_codec_select(&c,PP_CODEC_OFF)==0); off(&c);
    puts("Voice codec: PASS (10000 turns, one live allocation, failure rollback, frame bounds)");
}
