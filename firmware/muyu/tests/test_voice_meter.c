#include "pp_voice_meter.h"
/* Keep the checks active when a host compiler selects a release runtime. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void reading(const pp_voice_meter_t *meter,uint64_t ms,uint16_t level,uint16_t age)
{
    uint16_t actual_level=123,actual_age=456;
    pp_voice_meter_read(meter,ms,&actual_level,&actual_age);
    assert(actual_level==level && actual_age==age);
}

static void amplitudes(void)
{
    pp_voice_meter_t meter={0};
    int16_t pcm[PP_VOICE_METER_MAX_BYTES/2];
    reading(&meter,0,0,UINT16_MAX);
    for(unsigned i=0;i<PP_VOICE_METER_MAX_BYTES/2;++i) pcm[i]=INT16_MIN;
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,0));
    reading(&meter,0,32768,0); /* Time zero is a valid sample, not a sentinel. */
    for(unsigned i=0;i<PP_VOICE_METER_MAX_BYTES/2;++i) pcm[i]=INT16_MAX;
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,1));
    reading(&meter,1,32767,0);
    const int16_t mixed[]={INT16_MIN,INT16_MAX,0,-1};
    assert(pp_voice_meter_submit(&meter,mixed,sizeof(mixed),true,2));
    reading(&meter,2,16384,0); /* Average of absolute values, not signed mean. */
    const int16_t fractional[]={1,0,1};
    assert(pp_voice_meter_submit(&meter,fractional,sizeof(fractional),true,3));
    reading(&meter,3,0,0); /* Integer division deliberately rounds down. */
    for(unsigned i=0;i<PP_VOICE_METER_MAX_BYTES/2;++i) pcm[i]=0;
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,4));
    reading(&meter,5,0,1); /* Submitted silence is fresh, with zero amplitude. */
}

static void bounds_and_failed_writes(void)
{
    pp_voice_meter_t meter={0};
    const int16_t pcm[]={-1200,1200};
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,100));
    assert(!pp_voice_meter_submit(&meter,pcm,sizeof(pcm),false,200));
    reading(&meter,200,1200,100); /* A failed write must not refresh age. */
    assert(!pp_voice_meter_submit(&meter,NULL,2,true,201));
    assert(!pp_voice_meter_submit(&meter,pcm,0,true,201));
    assert(!pp_voice_meter_submit(&meter,pcm,1,true,201));
    assert(!pp_voice_meter_submit(&meter,pcm,3,true,201));
    /* Reject the size before reading this deliberately much smaller buffer. */
    assert(!pp_voice_meter_submit(&meter,pcm,PP_VOICE_METER_MAX_BYTES+2,true,201));
    assert(!pp_voice_meter_submit(&meter,pcm,SIZE_MAX,true,201));
    assert(!pp_voice_meter_submit(NULL,pcm,sizeof(pcm),true,201));
    reading(&meter,201,1200,101);
    pp_voice_meter_clear(&meter); /* Used by stop/error/session revocation. */
    reading(&meter,202,0,UINT16_MAX);
    assert(!pp_voice_meter_submit(&meter,NULL,SIZE_MAX,false,203));
    reading(&meter,203,0,UINT16_MAX);
    pp_voice_meter_clear(NULL);
    reading(NULL,203,0,UINT16_MAX);
    uint16_t output=99;
    pp_voice_meter_read(&meter,203,NULL,&output);
    pp_voice_meter_read(&meter,203,&output,NULL);
    assert(output==99);
}

static void monotonic_age(void)
{
    pp_voice_meter_t meter={0};
    const int16_t pcm[]={-2048};
    const uint64_t start=(uint64_t)UINT32_MAX+1000;
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,start));
    reading(&meter,start,2048,0);
    reading(&meter,start+PP_VOICE_METER_STALE_MS-1,2048,PP_VOICE_METER_STALE_MS-1);
    reading(&meter,start+PP_VOICE_METER_STALE_MS,0,PP_VOICE_METER_STALE_MS);
    reading(&meter,start+UINT16_MAX,0,UINT16_MAX);
    reading(&meter,start+UINT16_MAX+1,0,UINT16_MAX);
    reading(&meter,UINT64_MAX,0,UINT16_MAX);
    reading(&meter,start-1,0,UINT16_MAX); /* No unsigned underflow/revival. */
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,UINT64_MAX-10));
    reading(&meter,UINT64_MAX,2048,10);
    reading(&meter,0,0,UINT16_MAX); /* Hypothetical wrap fails closed. */
    assert(pp_voice_meter_submit(&meter,pcm,sizeof(pcm),true,0));
    reading(&meter,UINT64_MAX,0,UINT16_MAX);
}

static void repeated_turns(void)
{
    pp_voice_meter_t meter={0};
    const int16_t loud[2]={16000,-16000},silence[2]={0,0};
    for(unsigned turn=0;turn<10000;++turn) {
        uint64_t at=(uint64_t)turn*1000;
        assert(pp_voice_meter_submit(&meter,loud,sizeof(loud),true,at));
        reading(&meter,at+10,16000,10);
        assert(!pp_voice_meter_submit(&meter,silence,sizeof(silence),false,at+20));
        reading(&meter,at+20,16000,20);
        assert(pp_voice_meter_submit(&meter,silence,sizeof(silence),true,at+30));
        reading(&meter,at+30,0,0);
        pp_voice_meter_clear(&meter);
        reading(&meter,at+40,0,UINT16_MAX);
    }
}

int main(void)
{
    amplitudes(); bounds_and_failed_writes(); monotonic_age(); repeated_turns();
    printf("Voice meter PASS: amplitudes/bounds/clock/10000 turns; state=%zu bytes\n",sizeof(pp_voice_meter_t));
    return 0;
}
