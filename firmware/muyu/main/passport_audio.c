#include "passport_audio.h"
#include "bsp_audio.h"
#include <stdatomic.h>

/* System volume and a single hardware lease. No permanently resident tone
 * task or waveform. Only the owner may read/write the BSP codec. */
static atomic_bool owned, ready;
static atomic_uint volume;
static unsigned applied=101;
static bool hardware_attempted, hardware_ok;
bool pp_audio_init(unsigned value) { atomic_store(&volume,value); return true; }
void pp_audio_volume(unsigned value) { atomic_store(&volume,value>100?100:value); }
bool pp_audio_ok(void) { return atomic_load(&ready); }
bool pp_audio_acquire(void)
{
    bool expected=false;
    if(!atomic_compare_exchange_strong(&owned,&expected,true)) return false;
    if(!hardware_attempted) {
        hardware_attempted=true; hardware_ok=bsp_audio_init()==ESP_OK;
    }
    bool ok=hardware_ok && bsp_audio_set_format(16000,16,1)==ESP_OK;
    atomic_store(&ready,ok);
    if(!ok) { atomic_store(&owned,false); return false; }
    applied=101; pp_audio_apply_volume(); return true;
}
void pp_audio_apply_volume(void)
{
    unsigned value=atomic_load(&volume);
    if(value!=applied) { bsp_audio_set_volume((uint8_t)value); applied=value; }
}
void pp_audio_release(void)
{
    if(bsp_audio_suspend()!=ESP_OK) atomic_store(&ready,false);
    atomic_store(&owned,false);
}
