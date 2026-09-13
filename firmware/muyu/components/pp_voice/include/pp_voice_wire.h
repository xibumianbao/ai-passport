#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PP_VOICE_MESSAGE_MAX 8192
#define PP_VOICE_OPUS_MAX 1275

/* RFC 6455 messages may span network reads and continuation frames. */
typedef struct {
    size_t used, frame_used, frame_size;
    unsigned type;
    bool in_frame, more;
    uint8_t data[PP_VOICE_MESSAGE_MAX + 1];
} pp_voice_assembly_t;
/* -1 invalid/oversize, 0 incomplete/control, 1 complete text/binary message. */
int pp_voice_assemble(pp_voice_assembly_t *s, unsigned opcode, bool fin,
                      size_t frame_size, const void *data, size_t length);
bool pp_voice_unpack(unsigned version, const uint8_t *packet, size_t length,
                     const uint8_t **opus, size_t *opus_length);
size_t pp_voice_pack(unsigned version, uint32_t timestamp, const void *opus,
                     size_t length, uint8_t *out, size_t capacity);
bool pp_voice_wss_url(const char *url, char *host, size_t host_size,
                      unsigned *port, char *path, size_t path_size);
bool pp_voice_uuid_valid(const char *uuid);
bool pp_voice_header_value(const char *value, size_t capacity);
/* Bound parser recursion before passing an untrusted object to cJSON. */
bool pp_voice_json_safe(const void *data, size_t length);
