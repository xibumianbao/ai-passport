#include "pp_voice_wire.h"
#include <string.h>

int pp_voice_assemble(pp_voice_assembly_t *s, unsigned opcode, bool fin,
                      size_t frame_size, const void *data, size_t length)
{
    if(opcode >= 8) return opcode <= 10 && fin && frame_size <= 125 ? 0 : -1;
    if(!s->in_frame) {
        if(opcode == 1 || opcode == 2) {
            if(s->more) return -1;
            s->used=0; s->type=opcode;
        } else if(opcode != 0 || !s->more) return -1;
        if(frame_size > PP_VOICE_MESSAGE_MAX - s->used) return -1;
        s->frame_size=frame_size; s->frame_used=0; s->in_frame=true;
    }
    if(frame_size != s->frame_size || length > frame_size - s->frame_used ||
       length > PP_VOICE_MESSAGE_MAX - s->used || (!data && length)) return -1;
    if(length) memcpy(s->data+s->used,data,length);
    s->used+=length; s->frame_used+=length;
    if(s->frame_used != s->frame_size) return 0;
    s->in_frame=false; s->more=!fin;
    if(!fin) return 0;
    s->data[s->used]=0;
    return 1;
}
static uint32_t be32(const uint8_t *p)
{ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static void put32(uint8_t *p,uint32_t n)
{ p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
bool pp_voice_unpack(unsigned v,const uint8_t *p,size_t n,const uint8_t **out,size_t *len)
{
    if(!p || !out || !len) return false;
    size_t h=0;
    if(v==2) {
        h=16;
        if(n<h || p[0] || p[1]!=2 || p[2] || p[3] || be32(p+4) || be32(p+12)!=n-h) return false;
    } else if(v==3) {
        h=4;
        if(n<h || p[0] || p[1] || ((size_t)p[2]<<8 | p[3])!=n-h) return false;
    } else if(v!=1) return false;
    if(n<=h || n-h>PP_VOICE_OPUS_MAX) return false;
    *out=p+h; *len=n-h; return true;
}
size_t pp_voice_pack(unsigned v,uint32_t ts,const void *p,size_t n,uint8_t *out,size_t cap)
{
    size_t h=v==1?0:v==2?16:v==3?4:SIZE_MAX;
    if(!p || !out || !n || n>PP_VOICE_OPUS_MAX || h==SIZE_MAX || h+n>cap) return 0;
    memmove(out+h,p,n);
    memset(out,0,h);
    if(v==2) { out[1]=2; put32(out+8,ts); put32(out+12,(uint32_t)n); }
    if(v==3) { out[2]=n>>8; out[3]=n; }
    return h+n;
}
bool pp_voice_header_value(const char *s,size_t cap)
{
    if(!s) return false;
    for(size_t i=0;i<cap;++i) {
        unsigned char c=(unsigned char)s[i];
        if(!c) return true;
        if(c<32 || c==127) return false;
    }
    return false;
}
bool pp_voice_wss_url(const char *url,char *host,size_t hs,unsigned *port,char *path,size_t ps)
{
    if(!url || strncmp(url,"wss://",6) || !host || !port || !path) return false;
    const char *p=url+6, *end=p;
    while(*end && *end!='/' && *end!=':' && *end!='?') {
        unsigned char c=(unsigned char)*end;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return false;
        ++end;
    }
    size_t n=(size_t)(end-p);
    if(!n || n>=hs) return false;
    memcpy(host,p,n); host[n]=0; *port=443;
    if(*end==':') {
        ++end; unsigned v=0, digits=0;
        while(*end>='0'&&*end<='9') { if(++digits>5) return false; v=v*10+(unsigned)(*end++-'0'); }
        if(!digits || !v || v>65535) return false;
        *port=v;
    }
    if(!*end) end="/";
    if(*end!='/' || !pp_voice_header_value(end,ps)) return false;
    for(const char *q=end;*q;++q) if(*q=='#' || *q==' ') return false;
    n=strlen(end); if(n>=ps) return false;
    memcpy(path,end,n+1); return true;
}
bool pp_voice_uuid_valid(const char *s)
{
    if(!s || strlen(s)!=36) return false;
    for(unsigned i=0;i<36;++i) {
        if(i==8 || i==13 || i==18 || i==23) { if(s[i]!='-') return false; }
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F'))) return false;
    }
    return true;
}
