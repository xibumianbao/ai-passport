/* Xiaozhi protocol adapter. Protocol reference: FoloToy's MIT-licensed
 * d24fce080d86d7cc642f71585f6efde40fb99104; see docs/xiaozhi-standalone.md.
 * All I/O and codecs belong to one worker. It never calls LVGL or app runtime. */
#include "pp_voice.h"
#include "pp_voice_wire.h"
#include "passport_audio.h"
#include "passport_radio.h"
#include "passport_core.h"
#include "bsp_audio.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "esp_netif_sntp.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define WORKER_STACK 24576
#define PCM_BYTES 1920 /* 60 ms, 16 kHz, mono, signed 16-bit */
#define IO_TIMEOUT 1000
static const char *TAG="pp_voice";
static portMUX_TYPE snapshot_lock=portMUX_INITIALIZER_UNLOCKED;
static pp_voice_snapshot_t snapshot;
static atomic_uint wanted, pressed;
static atomic_bool busy;
static unsigned next_ticket, started_ticket;
typedef struct {
    unsigned ticket, version;
    char mac[18], uuid[37], url[384], token[1024], session[96];
    esp_transport_handle_t ssl, ws;
    pp_voice_assembly_t message;
    void *encoder, *decoder;
    uint8_t tx[PP_VOICE_OPUS_MAX+16], chunk[1024];
    int16_t pcm[1920]; /* max 120 ms output, decoder requests 60 ms */
    bool audio, hello, listening, response, tts;
    uint32_t tx_count, rx_count;
    int64_t last_rx, listen_at, response_at, last_ping;
} voice_t;
static bool live(const voice_t *v) { return atomic_load(&wanted)==v->ticket; }
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void publish(voice_t *v,pp_voice_state_t state,const char *detail)
{
    taskENTER_CRITICAL(&snapshot_lock);
    if(live(v)) {
        snapshot.state=state;
        snprintf(snapshot.detail,sizeof(snapshot.detail),"%s",detail);
        if(state!=PP_VOICE_ACTIVATION) snapshot.activation[0]=0;
        snapshot.tx_frames=v->tx_count; snapshot.rx_frames=v->rx_count;
        snapshot.free_heap=esp_get_free_heap_size();
    }
    taskEXIT_CRITICAL(&snapshot_lock);
}
static bool fail(voice_t *v,const char *detail)
{ publish(v,PP_VOICE_ERROR,detail); ESP_LOGW(TAG,"%s",detail); return false; }
void pp_voice_snapshot(pp_voice_snapshot_t *out)
{
    taskENTER_CRITICAL(&snapshot_lock); *out=snapshot; taskEXIT_CRITICAL(&snapshot_lock);
}
void pp_voice_open(void)
{
    if(++next_ticket==0) ++next_ticket;
    atomic_store(&pressed,0); atomic_store(&wanted,next_ticket);
    taskENTER_CRITICAL(&snapshot_lock);
    memset(&snapshot,0,sizeof(snapshot)); snapshot.state=PP_VOICE_STARTING;
    snprintf(snapshot.detail,sizeof(snapshot.detail),"%s",atomic_load(&busy)?"Closing previous session":"Starting voice service");
    taskEXIT_CRITICAL(&snapshot_lock);
    pp_voice_tick();
}
void pp_voice_close(void *unused)
{
    (void)unused; atomic_store(&wanted,0); atomic_store(&pressed,0);
    taskENTER_CRITICAL(&snapshot_lock); snapshot.state=PP_VOICE_OFF; taskEXIT_CRITICAL(&snapshot_lock);
}
void pp_voice_press(void)
{
    pp_voice_snapshot_t state; pp_voice_snapshot(&state);
    if(state.state==PP_VOICE_ERROR || state.state==PP_VOICE_WIFI) pp_voice_open();
    else if(state.state==PP_VOICE_READY || state.state==PP_VOICE_LISTENING ||
            state.state==PP_VOICE_THINKING || state.state==PP_VOICE_SPEAKING)
        atomic_store(&pressed,atomic_load(&wanted));
}
static const char *str(const cJSON *root,const char *key)
{
    const cJSON *s=cJSON_GetObjectItemCaseSensitive(root,key);
    return cJSON_IsString(s)?s->valuestring:NULL;
}
static bool copy_field(const cJSON *root,const char *key,char *out,size_t cap)
{
    const char *s=str(root,key);
    if(!s || !pp_voice_header_value(s,cap)) return false;
    memcpy(out,s,strlen(s)+1); return true;
}
static bool identity(voice_t *v)
{
    uint8_t mac[6];
    if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK) return fail(v,"Cannot read device identity");
    snprintf(v->mac,sizeof(v->mac),"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    nvs_handle_t nvs;
    if(nvs_open_from_partition("settings","svc_xiaozhi",NVS_READWRITE,&nvs)!=ESP_OK)
        return fail(v,"Identity storage unavailable");
    size_t n=sizeof(v->uuid); esp_err_t e=nvs_get_str(nvs,"uuid",v->uuid,&n);
    if(e==ESP_ERR_NVS_NOT_FOUND) {
        /* Import upstream identity only if it is still present. Full BIN
         * updates preserve our settings partition but may replace default NVS. */
        nvs_handle_t old; bool imported=false;
        if(nvs_open("board",NVS_READONLY,&old)==ESP_OK) {
            n=sizeof(v->uuid);
            imported=nvs_get_str(old,"uuid",v->uuid,&n)==ESP_OK && pp_voice_uuid_valid(v->uuid);
            nvs_close(old);
        }
        if(!imported) {
            uint8_t b[16]; esp_fill_random(b,sizeof(b)); b[6]=(b[6]&15)|64; b[8]=(b[8]&63)|128;
            snprintf(v->uuid,sizeof(v->uuid),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
        }
        e=nvs_set_str(nvs,"uuid",v->uuid);
        if(e==ESP_OK) e=nvs_commit(nvs);
    }
    nvs_close(nvs);
    return (e==ESP_OK && pp_voice_uuid_valid(v->uuid)) || fail(v,"Identity storage error");
}
static bool clock_ready(voice_t *v)
{
    static bool sntp_started;
    if(time(NULL)>=1735689600) return true;
    publish(v,PP_VOICE_CLOCK,"Synchronizing secure clock");
    if(!sntp_started) {
        esp_sntp_config_t cfg=ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
        cfg.num_of_servers=2; cfg.servers[1]="time.cloudflare.com";
        if(esp_netif_sntp_init(&cfg)!=ESP_OK) return fail(v,"Clock service unavailable");
        sntp_started=true;
    }
    int64_t until=now_ms()+15000;
    while(live(v) && time(NULL)<1735689600 && now_ms()<until) vTaskDelay(pdMS_TO_TICKS(100));
    return live(v) && (time(NULL)>=1735689600 || fail(v,"Clock sync failed. OK retry"));
}
/* Fixed-size response, no redirects or OTA writes, no credential logging.
 * A cancelled request may finish, but its response cannot start a session. */
static int http_post(voice_t *v,const char *url,const char *body,char *out,size_t cap)
{
    esp_http_client_config_t cfg={.url=url,.timeout_ms=5000,
        .crt_bundle_attach=esp_crt_bundle_attach,.disable_auto_redirect=true,
        .buffer_size=1024,.buffer_size_tx=1024};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    if(!h) return -1;
    esp_http_client_set_method(h,HTTP_METHOD_POST);
    esp_http_client_set_header(h,"Content-Type","application/json");
    esp_http_client_set_header(h,"Activation-Version","1");
    esp_http_client_set_header(h,"Device-Id",v->mac);
    esp_http_client_set_header(h,"Client-Id",v->uuid);
    esp_http_client_set_header(h,"Accept-Language","zh-CN");
    esp_http_client_set_header(h,"User-Agent","ai-passport-platform/" PP_VERSION);
    int status=-1; size_t used=0, body_length=strlen(body);
    if(live(v) && esp_http_client_open(h,(int)body_length)==ESP_OK &&
       esp_http_client_write(h,body,(int)body_length)==(int)body_length &&
       esp_http_client_fetch_headers(h)>=0) {
        status=esp_http_client_get_status_code(h);
        while(live(v) && used<cap-1) {
            int r=esp_http_client_read(h,out+used,(int)(cap-1-used));
            if(r<0) { status=-1; break; }
            if(!r) break;
            used+=(size_t)r;
        }
        if(!esp_http_client_is_complete_data_received(h)) status=-1;
    }
    out[used]=0;
    esp_http_client_close(h); esp_http_client_cleanup(h);
    return live(v)?status:-1;
}
static bool discover(voice_t *v)
{
    char body[768];
    snprintf(body,sizeof(body),"{\"version\":2,\"language\":\"zh-CN\",\"flash_size\":8388608,\"psram_size\":0,"
        "\"mac_address\":\"%s\",\"uuid\":\"%s\",\"chip_model_name\":\"esp32c3\","
        "\"application\":{\"name\":\"passport-platform\",\"version\":\"" PP_VERSION "\",\"idf_version\":\"5.5.3\"},"
        "\"board\":{\"type\":\"folo-ai-passport-c3\",\"name\":\"AI Passport\",\"mac\":\"%s\"}}",v->mac,v->uuid,v->mac);
    char *response=malloc(PP_VOICE_MESSAGE_MAX+1);
    if(!response) return fail(v,"Not enough memory for discovery");
    bool ok=false;
    int64_t activation_deadline=now_ms()+180000;
    while(live(v)) {
        publish(v,PP_VOICE_DISCOVERY,"Checking Xiaozhi binding");
        int status=http_post(v,OTA_URL,body,response,PP_VOICE_MESSAGE_MAX+1);
        if(status!=200) { fail(v,"Discovery failed. OK retry"); break; }
        cJSON *root=pp_voice_json_safe(response,strlen(response))?cJSON_Parse(response):NULL;
        if(!cJSON_IsObject(root)) { cJSON_Delete(root); fail(v,"Invalid discovery response"); break; }
        const cJSON *act=cJSON_GetObjectItemCaseSensitive(root,"activation");
        const char *code=str(act,"code");
        bool activate=code && *code;
        if(activate) {
            bool valid=strlen(code)<sizeof(snapshot.activation);
            for(const char *p=code;*p && valid;++p) valid=*p>='0' && *p<='9';
            if(!valid) { cJSON_Delete(root); fail(v,"Invalid activation code"); break; }
            publish(v,PP_VOICE_ACTIVATION,"Add this code at xiaozhi.me");
            taskENTER_CRITICAL(&snapshot_lock);
            if(live(v)) snprintf(snapshot.activation,sizeof(snapshot.activation),"%s",code);
            taskEXIT_CRITICAL(&snapshot_lock);
        } else {
            const cJSON *ws=cJSON_GetObjectItemCaseSensitive(root,"websocket");
            const cJSON *version=cJSON_GetObjectItemCaseSensitive(ws,"version");
            v->version=cJSON_IsNumber(version)?(unsigned)version->valueint:1;
            v->token[0]=0;
            ok=copy_field(ws,"url",v->url,sizeof(v->url)) &&
                (!str(ws,"token") || copy_field(ws,"token",v->token,sizeof(v->token))) &&
                v->version>=1 && v->version<=3;
            if(!ok) fail(v,"No supported WSS configuration");
        }
        /* firmware.url and force are intentionally never consumed. */
        cJSON_Delete(root);
        if(!activate) break;
        bool bound=false;
        while(live(v) && now_ms()<activation_deadline) {
            for(unsigned i=0;i<20 && live(v);++i) vTaskDelay(pdMS_TO_TICKS(100));
            status=http_post(v,OTA_URL "activate","{}",response,PP_VOICE_MESSAGE_MAX+1);
            if(status==200) { bound=true; break; }
            if(status!=202) { fail(v,"Activation failed. OK retry"); break; }
        }
        if(!bound) { if(live(v) && now_ms()>=activation_deadline) fail(v,"Code expired. OK retry"); break; }
    }
    memset(response,0,PP_VOICE_MESSAGE_MAX+1); free(response);
    return live(v) && ok;
}
static bool send_text(voice_t *v,const char *s)
{
    size_t n=strlen(s);
    return live(v) && esp_transport_ws_send_raw(v->ws,WS_TRANSPORT_OPCODES_TEXT|WS_TRANSPORT_OPCODES_FIN,s,(int)n,IO_TIMEOUT)==(int)n;
}
static bool connect_ws(voice_t *v)
{
    char host[128],path[384]; unsigned port;
    if(!pp_voice_wss_url(v->url,host,sizeof(host),&port,path,sizeof(path))) return fail(v,"Invalid secure endpoint");
    publish(v,PP_VOICE_CONNECTING,"Connecting to Xiaozhi");
    v->ssl=esp_transport_ssl_init();
    if(!v->ssl) return fail(v,"Not enough memory for TLS");
    esp_transport_ssl_crt_bundle_attach(v->ssl,esp_crt_bundle_attach);
    v->ws=esp_transport_ws_init(v->ssl);
    if(!v->ws) return fail(v,"Not enough memory for transport");
    char *headers=malloc(1280);
    if(!headers) return fail(v,"Not enough memory for headers");
    int length=snprintf(headers,1280,"Device-Id: %s\r\nClient-Id: %s\r\nProtocol-Version: %u\r\n%s%s%s%s",
        v->mac,v->uuid,v->version,v->token[0]?"Authorization: ":"",
        v->token[0] && !strchr(v->token,' ')?"Bearer ":"",v->token,v->token[0]?"\r\n":"");
    esp_transport_ws_config_t cfg={.ws_path=path,.headers=headers,.propagate_control_frames=true,
        .user_agent="ai-passport-platform/" PP_VERSION};
    esp_err_t e=length>0 && length<1280?esp_transport_ws_set_config(v->ws,&cfg):ESP_FAIL;
    memset(headers,0,1280); free(headers);
    if(e!=ESP_OK || !live(v) || esp_transport_connect(v->ws,host,(int)port,5000)!=0)
        return fail(v,"Secure connection failed");
    char hello[256];
    snprintf(hello,sizeof(hello),"{\"type\":\"hello\",\"version\":%u,\"transport\":\"websocket\","
        "\"features\":{\"mcp\":false},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}",v->version);
    v->last_rx=v->last_ping=now_ms();
    return send_text(v,hello) || fail(v,"Cannot send voice handshake");
}
static bool codecs(voice_t *v)
{
    esp_opus_enc_config_t enc={.sample_rate=16000,.channel=1,.bits_per_sample=16,.bitrate=24000,
        .frame_duration=ESP_OPUS_ENC_FRAME_DURATION_60_MS,.application_mode=ESP_OPUS_ENC_APPLICATION_VOIP,
        .complexity=0,.enable_fec=false,.enable_dtx=false,.enable_vbr=true};
    esp_opus_dec_cfg_t dec={.sample_rate=16000,.channel=1,
        .frame_duration=ESP_OPUS_DEC_FRAME_DURATION_60_MS,.self_delimited=false};
    if(esp_opus_enc_open(&enc,sizeof(enc),&v->encoder)!=ESP_AUDIO_ERR_OK ||
       esp_opus_dec_open(&dec,sizeof(dec),&v->decoder)!=ESP_AUDIO_ERR_OK)
        return fail(v,"Not enough memory for Opus");
    int in_bytes=0,out_bytes=0;
    esp_opus_enc_get_frame_size(v->encoder,&in_bytes,&out_bytes);
    if(in_bytes!=PCM_BYTES || out_bytes>PP_VOICE_OPUS_MAX) return fail(v,"Unsupported codec frame size");
    if(!live(v) || !pp_audio_acquire()) return fail(v,"Audio device unavailable");
    v->audio=true;
    return true;
}
static bool silence(voice_t *v)
{
    if(!v->audio) return true;
    memset(v->pcm,0,sizeof(v->pcm));
    /* Cover the six 240-frame DMA descriptors; do not repeat the last voice
     * sample while idle. This also drains the already submitted final PCM. */
    return bsp_audio_write(v->pcm,3200)==ESP_OK;
}
static bool listen_command(voice_t *v,bool start)
{
    cJSON *j=cJSON_CreateObject();
    if(!j) return false;
    cJSON_AddStringToObject(j,"session_id",v->session);
    cJSON_AddStringToObject(j,"type","listen");
    cJSON_AddStringToObject(j,"state",start?"start":"stop");
    if(start) cJSON_AddStringToObject(j,"mode","manual");
    char *s=cJSON_PrintUnformatted(j); cJSON_Delete(j);
    bool ok=s && send_text(v,s); free(s);
    if(ok) {
        v->listening=start; v->response=!start; v->tts=false;
        if(start) { v->listen_at=now_ms(); esp_opus_enc_reset(v->encoder); }
        else v->response_at=now_ms();
        publish(v,start?PP_VOICE_LISTENING:PP_VOICE_THINKING,start?"Speak now. OK to send":"Waiting for your answer");
    }
    return ok;
}
static bool json_message(voice_t *v)
{
    if(!pp_voice_json_safe(v->message.data,v->message.used)) return false;
    cJSON *root=cJSON_ParseWithLength((const char *)v->message.data,v->message.used);
    const char *type=str(root,"type"); bool ok=type!=NULL;
    if(type && !strcmp(type,"hello")) {
        const char *transport=str(root,"transport");
        const cJSON *ap=cJSON_GetObjectItemCaseSensitive(root,"audio_params");
        const char *format=str(ap,"format");
        const cJSON *rate=cJSON_GetObjectItemCaseSensitive(ap,"sample_rate");
        const cJSON *duration=cJSON_GetObjectItemCaseSensitive(ap,"frame_duration");
        const cJSON *channels=cJSON_GetObjectItemCaseSensitive(ap,"channels");
        int sr=cJSON_IsNumber(rate)?rate->valueint:0;
        ok=!v->hello && transport && !strcmp(transport,"websocket") &&
            format && !strcmp(format,"opus") && (sr==8000 || sr==12000 || sr==16000 || sr==24000 || sr==48000) &&
            (!channels || (cJSON_IsNumber(channels) && channels->valueint==1)) &&
            (!duration || (cJSON_IsNumber(duration) && duration->valueint==60)) &&
            copy_field(root,"session_id",v->session,sizeof(v->session));
        if(ok) { v->hello=true; publish(v,PP_VOICE_READY,"OK to start talking"); }
    } else if(type && !strcmp(type,"tts")) {
        const char *state=str(root,"state");
        if(!v->hello || !state) ok=false;
        else if(v->response && !strcmp(state,"start")) {
            v->listening=false; v->tts=true; esp_opus_dec_reset(v->decoder);
        }
        else if(v->response && !strcmp(state,"stop")) {
            ok=silence(v); v->tts=v->response=false;
            if(ok) publish(v,PP_VOICE_READY,"OK to talk again");
        }
    } else if(type && !strcmp(type,"goodbye")) ok=false;
    else if(type && !strcmp(type,"error")) ok=false;
    /* STT, emotion and sentence text are transient cloud events. No transcript
     * is retained and no remote system/reboot/update commands are executed. */
    cJSON_Delete(root); return ok;
}
static bool play(voice_t *v)
{
    if(!v->hello) return false;
    const uint8_t *opus; size_t length;
    if(!pp_voice_unpack(v->version,v->message.data,v->message.used,&opus,&length)) return false;
    if(!v->response || !v->tts) return true;
    esp_audio_dec_in_raw_t in={.buffer=(uint8_t *)opus,.len=length,
        .frame_recover=ESP_AUDIO_DEC_RECOVERY_NONE};
    esp_audio_dec_out_frame_t out={.buffer=(uint8_t *)v->pcm,.len=sizeof(v->pcm)};
    esp_audio_dec_info_t info={0};
    if(esp_opus_dec_decode(v->decoder,&in,&out,&info)!=ESP_AUDIO_ERR_OK ||
       in.consumed!=length || !out.decoded_size || out.decoded_size>sizeof(v->pcm) || out.decoded_size%2) return false;
    /* Opus supports decoder output at 16 kHz even for a 24 kHz sender. Keep
     * I2S at one format; no extra resampler, no clock changes during a turn. */
    for(size_t offset=0;offset<out.decoded_size && live(v);offset+=320) {
        size_t n=out.decoded_size-offset; if(n>320) n=320;
        if(bsp_audio_write((uint8_t *)v->pcm+offset,n)!=ESP_OK) return false;
    }
    if(!live(v)) return false;
    ++v->rx_count; publish(v,PP_VOICE_SPEAKING,"OK to stop the answer"); return true;
}
static bool receive(voice_t *v)
{
    /* Do not add a receive timeout to every 60 ms capture frame. Once hello
     * is consumed, poll TLS first; an incomplete frame must finish before
     * capturing another microphone frame. */
    if(v->hello && !v->message.in_frame) {
        int ready=esp_transport_poll_read(v->ws,0);
        if(ready<=0) return ready==0;
    }
    int n=esp_transport_read(v->ws,(char *)v->chunk,sizeof(v->chunk),IO_TIMEOUT);
    if(n<0) return false;
    unsigned op=(unsigned)esp_transport_ws_get_read_opcode(v->ws);
    if(!n && op==WS_TRANSPORT_OPCODES_NONE) return true;
    int size=esp_transport_ws_get_read_payload_len(v->ws);
    bool fin=esp_transport_ws_get_fin_flag(v->ws);
    if(op>=8) {
        if(size<0 || size>125 || n!=size || !fin) return false;
        if(op==WS_TRANSPORT_OPCODES_CLOSE) return false;
        if(op==WS_TRANSPORT_OPCODES_PING && esp_transport_ws_send_raw(v->ws,
            WS_TRANSPORT_OPCODES_PONG|WS_TRANSPORT_OPCODES_FIN,(char *)v->chunk,n,IO_TIMEOUT)!=n) return false;
        if(op!=WS_TRANSPORT_OPCODES_PING && op!=WS_TRANSPORT_OPCODES_PONG) return false;
        v->last_rx=now_ms(); return true;
    }
    if(n==0 && size!=0) return true;
    if(size<0) return false;
    int done=pp_voice_assemble(&v->message,op,fin,(size_t)size,v->chunk,(size_t)n);
    if(done<0) return false;
    if(done==0) return true;
    v->last_rx=now_ms();
    return v->message.type==1?json_message(v):play(v);
}
static bool capture(voice_t *v)
{
    /* Chunked reads bound cancellation latency; never record after revocation. */
    for(unsigned i=0;i<PCM_BYTES/320 && live(v);++i)
        if(bsp_audio_read((uint8_t *)v->pcm+i*320,320)!=ESP_OK) return false;
    if(!live(v)) return false;
    uint32_t level=0;
    for(unsigned i=0;i<PCM_BYTES/2;++i) level+=(unsigned)abs(v->pcm[i]);
    esp_audio_enc_in_frame_t in={.buffer=(uint8_t *)v->pcm,.len=PCM_BYTES};
    esp_audio_enc_out_frame_t out={.buffer=v->tx+16,.len=PP_VOICE_OPUS_MAX};
    if(esp_opus_enc_process(v->encoder,&in,&out)!=ESP_AUDIO_ERR_OK) return false;
    size_t n=pp_voice_pack(v->version,v->tx_count*60,v->tx+16,out.encoded_bytes,v->tx,sizeof(v->tx));
    if(!n || !live(v) || esp_transport_ws_send_raw(v->ws,WS_TRANSPORT_OPCODES_BINARY|WS_TRANSPORT_OPCODES_FIN,
        (char *)v->tx,(int)n,IO_TIMEOUT)!=(int)n) return false;
    ++v->tx_count;
    taskENTER_CRITICAL(&snapshot_lock);
    if(live(v)) { snapshot.tx_frames=v->tx_count; snapshot.level=(uint16_t)(level/(PCM_BYTES/2)); }
    taskEXIT_CRITICAL(&snapshot_lock);
    return true;
}
static void worker(void *arg)
{
    voice_t *v=arg;
    esp_log_level_set("TRANSPORT_WS",ESP_LOG_WARN);
    esp_log_level_set("HTTP_CLIENT",ESP_LOG_WARN);
    pp_radio_state_t radio;
    int64_t wifi_started=now_ms();
    while(live(v)) {
        pp_radio_snapshot(&radio);
        if(radio.wifi==PP_WIFI_ONLINE) break;
        int64_t elapsed=now_ms()-wifi_started;
        if(elapsed>=30000 || (elapsed>=1500 && radio.wifi!=PP_WIFI_CONNECTING)) {
            publish(v,PP_VOICE_WIFI,"Connect Wi-Fi in Settings"); goto done;
        }
        publish(v,PP_VOICE_STARTING,"Waiting for system Wi-Fi"); vTaskDelay(pdMS_TO_TICKS(100));
    }
    if(!live(v)) goto done;
    if(!identity(v) || !clock_ready(v) || !discover(v) || !connect_ws(v) || !codecs(v)) goto done;
    while(live(v)) {
        if(!receive(v)) { fail(v,"Connection ended. OK retry"); break; }
        int64_t now=now_ms();
        if((!v->hello && now-v->last_rx>10000) || now-v->last_rx>90000 ||
           (v->response && now-v->response_at>60000)) { fail(v,"Voice timed out. OK retry"); break; }
        if(atomic_exchange(&pressed,0)==v->ticket) {
            if(v->response) {
                /* Close the transport to prevent a late old answer being
                 * mistaken for a response to a new turn. */
                silence(v); publish(v,PP_VOICE_ERROR,"Answer stopped. OK reconnect"); break;
            }
            if(v->hello && !listen_command(v,!v->listening)) { fail(v,"Cannot send listen command"); break; }
        }
        if(v->listening && now-v->listen_at>=30000 && !listen_command(v,false)) { fail(v,"Cannot finish recording"); break; }
        pp_audio_apply_volume();
        if(v->listening && !v->message.in_frame && !capture(v)) { if(live(v)) fail(v,"Recording failed. OK retry"); break; }
        if(now-v->last_ping>=20000) {
            if(esp_transport_ws_send_raw(v->ws,WS_TRANSPORT_OPCODES_PING|WS_TRANSPORT_OPCODES_FIN,"",0,IO_TIMEOUT)<0) {
                fail(v,"Keepalive failed. OK retry"); break;
            }
            v->last_ping=now;
        }
        taskENTER_CRITICAL(&snapshot_lock);
        if(live(v)) { snapshot.free_heap=esp_get_free_heap_size(); snapshot.stack_free=uxTaskGetStackHighWaterMark(NULL); }
        taskEXIT_CRITICAL(&snapshot_lock);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
done:
    if(v->audio) { silence(v); pp_audio_release(); }
    if(v->encoder) esp_opus_enc_close(v->encoder);
    if(v->decoder) esp_opus_dec_close(v->decoder);
    if(v->ws) { esp_transport_close(v->ws); esp_transport_destroy(v->ws); }
    if(v->ssl) esp_transport_destroy(v->ssl);
    ESP_LOGI(TAG,"Session closed: tx=%lu rx=%lu heap=%lu stack=%u",
        (unsigned long)v->tx_count,(unsigned long)v->rx_count,(unsigned long)esp_get_free_heap_size(),
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
    memset(v,0,sizeof(*v)); free(v);
    atomic_store(&busy,false); vTaskDelete(NULL);
}
void pp_voice_tick(void)
{
    unsigned ticket=atomic_load(&wanted);
    if(!ticket || started_ticket==ticket || atomic_load(&busy)) return;
    started_ticket=ticket;
    voice_t *v=calloc(1,sizeof(*v));
    if(!v) {
        taskENTER_CRITICAL(&snapshot_lock); snapshot.state=PP_VOICE_ERROR;
        snprintf(snapshot.detail,sizeof(snapshot.detail),"Not enough memory for voice"); taskEXIT_CRITICAL(&snapshot_lock); return;
    }
    v->ticket=ticket; atomic_store(&busy,true);
    if(xTaskCreate(worker,"xiaozhi",WORKER_STACK,v,5,NULL)!=pdPASS) {
        fail(v,"Not enough memory for worker"); free(v); atomic_store(&busy,false);
    }
}
