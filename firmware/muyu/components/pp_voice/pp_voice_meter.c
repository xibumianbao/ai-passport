#include "pp_voice_meter.h"

bool pp_voice_meter_submit(pp_voice_meter_t *meter,const int16_t *pcm,
    size_t bytes,bool written,uint64_t now_ms)
{
    if(!meter || !written || !pcm || !bytes || bytes%2 || bytes>PP_VOICE_METER_MAX_BYTES)
        return false;
    uint32_t sum=0;
    const size_t samples=bytes/2;
    for(size_t i=0;i<samples;++i) {
        /* Promote before negating: abs(INT16_MIN) is 32768, not -32768. The
         * bounded sum is at most 1920*32768 and fits in uint32_t. */
        int32_t sample=pcm[i];
        sum+=(uint32_t)(sample<0?-sample:sample);
    }
    *meter=(pp_voice_meter_t){.at_ms=now_ms,.level=(uint16_t)(sum/samples),.valid=true};
    return true;
}

void pp_voice_meter_clear(pp_voice_meter_t *meter)
{
    if(meter) *meter=(pp_voice_meter_t){0};
}

void pp_voice_meter_read(const pp_voice_meter_t *meter,uint64_t now_ms,
    uint16_t *level,uint16_t *age_ms)
{
    if(!level || !age_ms) return;
    *level=0; *age_ms=UINT16_MAX;
    if(!meter || !meter->valid || now_ms<meter->at_ms) return;
    uint64_t elapsed=now_ms-meter->at_ms;
    *age_ms=elapsed>UINT16_MAX?UINT16_MAX:(uint16_t)elapsed;
    if(elapsed<PP_VOICE_METER_STALE_MS) *level=meter->level;
}
