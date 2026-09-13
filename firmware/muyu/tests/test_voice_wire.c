#include "pp_voice_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void framing(void)
{
    uint8_t data[PP_VOICE_OPUS_MAX], wire[PP_VOICE_OPUS_MAX+16];
    for(size_t i=0;i<sizeof(data);++i) data[i]=(uint8_t)i;
    for(unsigned v=1;v<=3;++v) for(size_t n=1;n<=sizeof(data);++n) {
        size_t count=pp_voice_pack(v,0x12345678,data,n,wire,sizeof(wire));
        const uint8_t *out=NULL; size_t len=0;
        assert(count && pp_voice_unpack(v,wire,count,&out,&len));
        assert(len==n && !memcmp(data,out,n));
        assert(!pp_voice_pack(v,0,data,n,wire,count-1));
        if(v>1) {
            assert(!pp_voice_unpack(v,wire,count-1,&out,&len));
            wire[0]=1; assert(!pp_voice_unpack(v,wire,count,&out,&len));
        }
    }
    const uint8_t *out; size_t len;
    assert(!pp_voice_unpack(0,data,1,&out,&len));
    assert(!pp_voice_unpack(1,data,0,&out,&len));
    assert(!pp_voice_pack(4,0,data,1,wire,sizeof(wire)));
    assert(!pp_voice_pack(1,0,data,0,wire,sizeof(wire)));
    assert(!pp_voice_pack(1,0,data,PP_VOICE_OPUS_MAX+1,wire,sizeof(wire)));
    /* Encoder output is already inside the destination array. */
    memcpy(wire+16,data,100);
    assert(pp_voice_pack(1,0,wire+16,100,wire,sizeof(wire))==100);
    assert(!memcmp(wire,data,100));
}
static void fragments(void)
{
    pp_voice_assembly_t s={0};
    assert(pp_voice_assemble(&s,0,true,1,"x",1)==-1);
    memset(&s,0,sizeof(s));
    assert(pp_voice_assemble(&s,1,false,3,"ab",2)==0);
    assert(pp_voice_assemble(&s,1,false,3,"c",1)==0);
    assert(pp_voice_assemble(&s,9,true,1,"p",1)==0); /* interleaved ping */
    assert(pp_voice_assemble(&s,0,true,2,"de",2)==1);
    assert(s.type==1 && s.used==5 && !strcmp((char *)s.data,"abcde"));
    assert(pp_voice_assemble(&s,2,true,1,"X",1)==1 && s.used==1);
    memset(&s,0,sizeof(s));
    assert(pp_voice_assemble(&s,2,false,1,"x",1)==0);
    assert(pp_voice_assemble(&s,1,true,1,"x",1)==-1);
    assert(pp_voice_assemble(&s,0,true,PP_VOICE_MESSAGE_MAX,"x",1)==-1);
    memset(&s,0,sizeof(s));
    assert(pp_voice_assemble(&s,1,true,1,"xx",2)==-1);
    assert(pp_voice_assemble(&s,9,false,0,NULL,0)==-1);
    assert(pp_voice_assemble(&s,10,true,126,NULL,0)==-1);
    assert(pp_voice_assemble(&s,11,true,0,NULL,0)==-1);
    /* Every split point in a large JSON message, plus exact capacity. */
    for(size_t split=0;split<=PP_VOICE_MESSAGE_MAX;++split) {
        memset(&s,0,sizeof(s)); static char bytes[PP_VOICE_MESSAGE_MAX];
        int done=pp_voice_assemble(&s,1,true,sizeof(bytes),bytes,split);
        if(split<sizeof(bytes)) {
            assert(done==0);
            assert(pp_voice_assemble(&s,1,true,sizeof(bytes),bytes+split,sizeof(bytes)-split)==1);
        } else assert(done==1);
        assert(s.used==PP_VOICE_MESSAGE_MAX && s.data[s.used]==0);
    }
}
static void config(void)
{
    char host[128],path[128]; unsigned port;
    assert(pp_voice_wss_url("wss://example.com:8443/chat?v=2",host,sizeof(host),&port,path,sizeof(path)));
    assert(!strcmp(host,"example.com") && port==8443 && !strcmp(path,"/chat?v=2"));
    assert(pp_voice_wss_url("wss://example.com",host,sizeof(host),&port,path,sizeof(path)) && port==443 && !strcmp(path,"/"));
    const char *bad[]={"ws://example.com/","https://example.com/","wss:///x","wss://u:p@example.com/",
        "wss://example.com:0/","wss://example.com:65536/","wss://example.com:9999999999999/",
        "wss://example.com/x\r\nHost:evil","wss://example.com/x#f","wss://example.com/a b"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) assert(!pp_voice_wss_url(bad[i],host,sizeof(host),&port,path,sizeof(path)));
    assert(!pp_voice_wss_url("wss://example.com",host,2,&port,path,sizeof(path)));
    assert(!pp_voice_header_value("x\ny",100)); assert(!pp_voice_header_value("xxx",3));
    assert(pp_voice_uuid_valid("12345678-abcd-4321-8123-123456789abc"));
    assert(!pp_voice_uuid_valid("12345678Xabcd-4321-8123-123456789abc"));
    assert(!pp_voice_uuid_valid("12345678-abcd-4321-8123-123456789abg"));
}
int main(void) { framing(); fragments(); config(); puts("Voice wire bounds, fragmentation and configuration: PASS"); }
