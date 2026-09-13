/* Real orchestration with SDK stubs: validates bounds, cancellation and codec
 * ownership, NOT Espressif codec output, hardware RAM or stack requirements. */
#include "pp_voice_selftest.h"
#include "pp_voice_wire.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NORMAL,ENC_ERROR,DEC_ERROR,ENC_ZERO,ENC_LARGE,ENC_GUARD,PCM_GUARD,
    DEC_ZERO,DEC_LARGE,DEC_CONSUME,DEC_GUARD,DEC_ODD,DEC_PARTIAL,OPEN_FAIL };
static unsigned mode,active,opens,closes,encoded,decoded,observed,yields,cancel_at,refuse_at;
static unsigned pattern_mask;
static uint32_t checksum[4];
static uint64_t timer;
static bool allowed;
static uint32_t *allocate(unsigned kind)
{ assert(!active); uint32_t *p=malloc(sizeof(*p)); assert(p); *p=kind; active=kind; ++opens; return p; }
static void release(void *handle,unsigned kind)
{ assert(handle && active==kind && *(uint32_t *)handle==kind); free(handle); active=0; ++closes; }
esp_audio_err_t esp_opus_enc_open(void *cfg,uint32_t size,void **handle)
{
    esp_opus_enc_config_t *c=cfg;
    assert(size==sizeof(*c) && c->sample_rate==16000 && c->bitrate==24000 && c->complexity==0);
    assert(c->application_mode==ESP_OPUS_ENC_APPLICATION_VOIP && c->frame_duration==5 && !c->enable_dtx);
    if(mode==OPEN_FAIL) { *handle=NULL; return ESP_AUDIO_ERR_MEM_LACK; }
    *handle=allocate(1); return ESP_AUDIO_ERR_OK;
}
esp_audio_err_t esp_opus_dec_open(void *cfg,uint32_t size,void **handle)
{
    esp_opus_dec_cfg_t *c=cfg;
    assert(size==sizeof(*c) && c->sample_rate==16000 && c->channel==1);
    *handle=allocate(2); return ESP_AUDIO_ERR_OK;
}
void esp_opus_enc_close(void *h) { release(h,1); }
void esp_opus_dec_close(void *h) { release(h,2); }
esp_audio_err_t esp_opus_enc_get_frame_size(void *h,int *in,int *out)
{ assert(h && active==1); *in=PP_VOICE_PCM_BYTES; *out=PP_VOICE_OPUS_MAX; return ESP_AUDIO_ERR_OK; }
esp_audio_err_t esp_opus_enc_process(void *h,esp_audio_enc_in_frame_t *in,esp_audio_enc_out_frame_t *out)
{
    assert(h && active==1 && in->len==PP_VOICE_PCM_BYTES && out->len==PP_VOICE_OPUS_MAX);
    unsigned pattern=encoded/PP_VOICE_TEST_FRAMES;
    if(pattern<4) {
        for(unsigned i=0;i<in->len;++i) checksum[pattern]=checksum[pattern]*33+in->buffer[i];
        pattern_mask|=1u<<pattern;
    }
    ++encoded;
    memset(out->buffer,0x3c,10); out->encoded_bytes=10;
    if(mode==ENC_ZERO) out->encoded_bytes=0;
    if(mode==ENC_LARGE) out->encoded_bytes=PP_VOICE_OPUS_MAX+1;
    if(mode==ENC_GUARD) out->buffer[out->len]=0;
    if(mode==PCM_GUARD) in->buffer[in->len]=0;
    return mode==ENC_ERROR?ESP_AUDIO_ERR_FAIL:ESP_AUDIO_ERR_OK;
}
esp_audio_err_t esp_opus_dec_decode(void *h,esp_audio_dec_in_raw_t *in,esp_audio_dec_out_frame_t *out,esp_audio_dec_info_t *info)
{
    (void)info; assert(h && active==2 && in->len && in->buffer[0]==0x3c);
    ++decoded; in->consumed=in->len; out->decoded_size=out->len;
    if(mode==DEC_PARTIAL && in->len==10) { in->consumed=5; out->decoded_size=out->len/2; }
    memset(out->buffer,0x11,out->decoded_size);
    if(mode==DEC_ZERO) { in->consumed=0; out->decoded_size=0; }
    if(mode==DEC_LARGE) out->decoded_size=out->len+2;
    if(mode==DEC_CONSUME) in->consumed=in->len+1;
    if(mode==DEC_GUARD) out->buffer[out->len]=0;
    if(mode==DEC_ODD) { in->consumed=1; out->decoded_size=1; }
    return mode==DEC_ERROR?ESP_AUDIO_ERR_FAIL:ESP_AUDIO_ERR_OK;
}
static bool live(void *ctx) { (void)ctx; return allowed; }
static bool observe(void *ctx,const pp_voice_test_event_t *e)
{
    (void)ctx; ++observed;
    assert(e->pattern<4 && e->frames<=PP_VOICE_TEST_FRAMES);
    return !refuse_at || observed!=refuse_at;
}
static uint64_t now(void *ctx) { (void)ctx; return ++timer; }
static void pause_test(void *ctx)
{ (void)ctx; if(++yields==cancel_at) allowed=false; }
static void reset(unsigned failure)
{
    assert(!active && opens==closes);
    mode=failure; encoded=decoded=observed=yields=cancel_at=refuse_at=pattern_mask=0;
    memset(checksum,0,sizeof(checksum)); allowed=true;
}
static bool run(size_t pcm_size,size_t packet_size,size_t cache_size)
{
    int16_t pcm[1920]; uint8_t packet[PP_VOICE_OPUS_MAX+16],cache[PP_VOICE_TEST_FRAMES*PP_VOICE_OPUS_MAX];
    pp_voice_codec_t c={0};
    const pp_voice_test_hooks_t hooks={.live=live,.observe=observe,.now_us=now,.yield=pause_test};
    bool ok=pp_voice_selftest(&c,pcm,pcm_size,packet,packet_size,cache,cache_size,&hooks);
    assert(!c.handle && c.mode==PP_CODEC_OFF && !active && opens==closes);
    return ok;
}
static bool full(void) { return run(3840,PP_VOICE_OPUS_MAX+16,PP_VOICE_TEST_FRAMES*PP_VOICE_OPUS_MAX); }
int main(void)
{
    reset(NORMAL); assert(full());
    assert(encoded==16 && decoded==16 && opens==8 && pattern_mask==15);
    assert(checksum[0]==0 && checksum[1] && checksum[2] && checksum[3]);
    assert(checksum[1]!=checksum[2] && checksum[2]!=checksum[3]);
    for(unsigned m=ENC_ERROR;m<=OPEN_FAIL;++m) {
        reset(m); bool ok=full(); assert(ok==(m==DEC_PARTIAL));
        if(m==DEC_PARTIAL) assert(decoded==32);
        if(m==DEC_ODD) assert(decoded==1); /* refuse before a misaligned next call */
    }
    for(unsigned stop=1;stop<=32;++stop) {
        reset(NORMAL); cancel_at=stop; assert(!full());
    }
    for(unsigned refuse=1;refuse<=52;++refuse) {
        reset(NORMAL); refuse_at=refuse; assert(!full());
    }
    reset(NORMAL); assert(!run(PP_VOICE_PCM_BYTES+15,PP_VOICE_OPUS_MAX+16,5100));
    assert(!run(3840,PP_VOICE_OPUS_MAX+15,5100));
    assert(!run(3840,PP_VOICE_OPUS_MAX+16,5099));
    puts("Voice self-test orchestration: PASS (patterns, codec release, cancellation, partial decode, guards and bounds; SDK mocked)");
}
