#include "pp_pet_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t data[2][PP_PET_SAVE_SIZE];
static bool exists[2],opened;
static unsigned closes,commits,sets;
static int fail_open,fail_set,fail_commit,fail_read,fail_verify;
static int slot(const char *key)
{
    if(!strcmp(key,"save_a")) return 0;
    assert(!strcmp(key,"save_b")); return 1;
}
esp_err_t nvs_open_from_partition(const char *part,const char *ns,int mode,nvs_handle_t *h)
{
    assert(!strcmp(part,"settings") && !strcmp(ns,"app_pet") && mode==NVS_READWRITE);
    if(fail_open) return 10;
    *h=42; opened=true; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n)
{
    assert(h==42 && opened); int i=slot(key);
    if(fail_read || fail_verify) return 10;
    if(!exists[i]) return ESP_ERR_NVS_NOT_FOUND;
    if(*n<PP_PET_SAVE_SIZE) return ESP_ERR_NVS_INVALID_LENGTH;
    *n=PP_PET_SAVE_SIZE; memcpy(out,data[i],*n); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t n)
{
    assert(h==42 && opened && n==PP_PET_SAVE_SIZE); ++sets;
    if(fail_set) return 10;
    int i=slot(key); memcpy(data[i],value,n); exists[i]=true; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { assert(h==42 && opened); ++commits; return fail_commit?10:ESP_OK; }
void nvs_close(nvs_handle_t h) { assert(h==42 && opened); opened=false; ++closes; }
uint32_t esp_random(void) { return 77; }
static void reset(void)
{
    pp_pet_store_close(); memset(data,0,sizeof(data)); memset(exists,0,sizeof(exists));
    fail_open=fail_set=fail_commit=fail_read=fail_verify=0; sets=commits=closes=0;
}
int main(void)
{
    pp_pet_save_t s; uint8_t b[PP_PET_SAVE_SIZE];
    reset(); assert(pp_pet_store_open(&s)); assert(s.id==77 && s.coins==10 && sets==1 && commits==1);
    s.sequence++; s.coins=19; pp_pet_encode(&s,b); assert(pp_pet_store_write(NULL,b));
    pp_pet_store_close(); assert(!opened); assert(pp_pet_store_open(&s) && s.coins==19);
    assert(sets==2); pp_pet_store_close();
    data[1][20]^=1; assert(pp_pet_store_open(&s) && s.coins==10); pp_pet_store_close();
    data[0][20]^=1; unsigned before=sets; assert(!pp_pet_store_open(&s) && !opened && sets==before);
    reset(); fail_open=1; assert(!pp_pet_store_open(&s) && !sets && !opened);
    reset(); fail_read=1; assert(!pp_pet_store_open(&s) && !sets && !opened);
    reset(); fail_set=1; assert(!pp_pet_store_open(&s) && !opened);
    reset(); fail_commit=1; assert(!pp_pet_store_open(&s) && !opened);
    reset(); assert(pp_pet_store_open(&s)); s.sequence++; pp_pet_encode(&s,b);
    fail_set=1; assert(!pp_pet_store_write(NULL,b)); fail_set=0;
    fail_commit=1; assert(!pp_pet_store_write(NULL,b)); fail_commit=0;
    fail_verify=1; assert(!pp_pet_store_write(NULL,b)); fail_verify=0;
    assert(pp_pet_store_write(NULL,b)); pp_pet_store_close();
    assert(!pp_pet_store_write(NULL,b));
    puts("Pet NVS adapter: PASS (real adapter with bounded fake NVS; partition isolation, dual-record recovery, read/write/commit failures)");
    return fflush(stdout)==0?0:1;
}
