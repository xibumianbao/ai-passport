#include "pp_voice_selftest.h"
#include "pp_voice_wire.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include <string.h>

static void guards_set(uint8_t *buffer,size_t payload)
{
    memset(buffer,0x6b,PP_VOICE_TEST_GUARD);
    memset(buffer+PP_VOICE_TEST_GUARD+payload,0xb6,PP_VOICE_TEST_GUARD);
}
static bool guards_ok(const uint8_t *buffer,size_t payload)
{
    for(unsigned i=0;i<PP_VOICE_TEST_GUARD;++i)
        if(buffer[i]!=0x6b || buffer[PP_VOICE_TEST_GUARD+payload+i]!=0xb6) return false;
    return true;
}
static void pattern_fill(int16_t *pcm,unsigned pattern,uint32_t *noise)
{
    static const int16_t wave[]={0,4592,8485,11087,12000,11087,8485,4592,
        0,-4592,-8485,-11087,-12000,-11087,-8485,-4592};
    for(unsigned i=0;i<PP_VOICE_PCM_BYTES/2;++i) {
        if(pattern==0) pcm[i]=0;
        else if(pattern==1) pcm[i]=wave[i%16];
        else if(pattern==2) {
            *noise^=*noise<<13; *noise^=*noise>>17; *noise^=*noise<<5;
            pcm[i]=(int16_t)((int32_t)(*noise&0xffff)-32768);
        } else pcm[i]=(i/8)%2?32760:-32760;
    }
}
static bool observe(const pp_voice_test_hooks_t *h,const char *stage,unsigned pattern,
    unsigned frames,esp_audio_err_t error,size_t bytes,uint64_t started,bool valid,bool log,size_t consumed)
{
    pp_voice_test_event_t event={.stage=stage,.pattern=pattern,.frames=frames,
        .error=error,.bytes=bytes,.elapsed_us=(uint32_t)(h->now_us(h->context)-started),
        .consumed=consumed,.bounds_ok=valid,.log=log || error!=ESP_AUDIO_ERR_OK || !valid};
    return h->observe(h->context,&event) && h->live(h->context) &&
        error==ESP_AUDIO_ERR_OK && valid;
}
bool pp_voice_selftest(pp_voice_codec_t *codec,void *pcm,size_t pcm_size,
    uint8_t *packet,size_t packet_size,uint8_t *cache,size_t cache_size,
    const pp_voice_test_hooks_t *h)
{
    bool ok=false;
    if(!codec) return false;
    pp_voice_codec_close(codec);
    if(!pcm || !packet || !cache || !h || !h->live || !h->observe || !h->now_us || !h->yield ||
       (uintptr_t)pcm%_Alignof(int16_t) || pcm_size<PP_VOICE_PCM_BYTES+2*PP_VOICE_TEST_GUARD ||
       packet_size<PP_VOICE_OPUS_MAX+2*PP_VOICE_TEST_GUARD ||
       cache_size<PP_VOICE_TEST_FRAMES*PP_VOICE_OPUS_MAX) return false;
    uint8_t *sample=(uint8_t *)pcm+PP_VOICE_TEST_GUARD;
    uint8_t *encoded=packet+PP_VOICE_TEST_GUARD;
    for(unsigned pattern=0;pattern<PP_VOICE_TEST_PATTERNS;++pattern) {
        uint16_t lengths[PP_VOICE_TEST_FRAMES]={0};
        uint32_t noise=0x5a17c9e3;
        size_t used=0;
        if(!h->live(h->context)) goto done;
        uint64_t start=h->now_us(h->context);
        esp_audio_err_t error=pp_voice_codec_select(codec,PP_CODEC_ENCODE);
        if(!observe(h,"encoder open",pattern,0,error,0,start,true,true,0)) goto done;
        for(unsigned frame=0;frame<PP_VOICE_TEST_FRAMES;++frame) {
            if(!h->live(h->context)) goto done;
            guards_set(pcm,PP_VOICE_PCM_BYTES); guards_set(packet,PP_VOICE_OPUS_MAX);
            pattern_fill((int16_t *)sample,pattern,&noise);
            start=h->now_us(h->context);
            if(frame==0 && !observe(h,"encode begin",pattern,0,ESP_AUDIO_ERR_OK,0,start,true,true,0)) goto done;
            start=h->now_us(h->context);
            esp_audio_enc_in_frame_t input={.buffer=sample,.len=PP_VOICE_PCM_BYTES};
            esp_audio_enc_out_frame_t output={.buffer=encoded,.len=PP_VOICE_OPUS_MAX};
            error=esp_opus_enc_process(codec->handle,&input,&output);
            bool valid=guards_ok(pcm,PP_VOICE_PCM_BYTES) && guards_ok(packet,PP_VOICE_OPUS_MAX) &&
                output.encoded_bytes>0 && output.encoded_bytes<=PP_VOICE_OPUS_MAX &&
                output.encoded_bytes<=cache_size-used;
            if(!observe(h,"encode end",pattern,frame+1,error,output.encoded_bytes,start,valid,
                        frame==0 || frame+1==PP_VOICE_TEST_FRAMES,0)) goto done;
            lengths[frame]=(uint16_t)output.encoded_bytes;
            memcpy(cache+used,encoded,output.encoded_bytes); used+=output.encoded_bytes;
            h->yield(h->context);
        }
        pp_voice_codec_close(codec);
        if(!h->live(h->context)) goto done;
        start=h->now_us(h->context);
        error=pp_voice_codec_select(codec,PP_CODEC_DECODE);
        if(!observe(h,"decoder open",pattern,0,error,0,start,true,true,0)) goto done;
        size_t offset=0;
        for(unsigned frame=0;frame<PP_VOICE_TEST_FRAMES;++frame) {
            if(!h->live(h->context)) goto done;
            guards_set(pcm,PP_VOICE_PCM_BYTES);
            start=h->now_us(h->context);
            if(frame==0 && !observe(h,"decode begin",pattern,0,ESP_AUDIO_ERR_OK,0,start,true,true,0)) goto done;
            start=h->now_us(h->context);
            size_t consumed=0,decoded=0;
            bool valid=true;
            /* A direct decoder may consume a packet in pieces. Bound every
             * piece and require forward progress; never silently drop a tail. */
            do {
                if(!h->live(h->context)) goto done;
                esp_audio_dec_in_raw_t input={.buffer=cache+offset+consumed,.len=lengths[frame]-consumed,
                    .frame_recover=ESP_AUDIO_DEC_RECOVERY_NONE};
                esp_audio_dec_out_frame_t output={.buffer=sample+decoded,.len=PP_VOICE_PCM_BYTES-decoded};
                esp_audio_dec_info_t info={0};
                error=esp_opus_dec_decode(codec->handle,&input,&output,&info);
                valid=guards_ok(pcm,PP_VOICE_PCM_BYTES) && input.consumed<=input.len &&
                    output.decoded_size<=output.len && !(output.decoded_size%2) &&
                    (input.consumed || output.decoded_size);
                if(error!=ESP_AUDIO_ERR_OK || !valid) break;
                consumed+=input.consumed; decoded+=output.decoded_size;
            } while(consumed<lengths[frame] && decoded<PP_VOICE_PCM_BYTES);
            valid=valid && consumed==lengths[frame] && decoded==PP_VOICE_PCM_BYTES;
            if(!observe(h,"decode end",pattern,frame+1,error,decoded,start,valid,
                        frame==0 || frame+1==PP_VOICE_TEST_FRAMES,consumed)) goto done;
            offset+=lengths[frame]; h->yield(h->context);
        }
        pp_voice_codec_close(codec);
        start=h->now_us(h->context);
        if(!observe(h,"pattern released",pattern,PP_VOICE_TEST_FRAMES,ESP_AUDIO_ERR_OK,used,start,true,true,0)) goto done;
    }
    ok=true;
done:
    pp_voice_codec_close(codec);
    /* Clear test material before these buffers are reused by the live session. */
    memset(pcm,0,pcm_size); memset(packet,0,packet_size); memset(cache,0,cache_size);
    return ok && h->live(h->context);
}
