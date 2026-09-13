#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Shared presentation contract. Network/audio callbacks must queue events to
 * the control task; this API owns neither transport nor microphone/playback. */
typedef enum { AVATAR_IDLE, AVATAR_LISTENING, AVATAR_THINKING, AVATAR_SPEAKING,
               AVATAR_INTERRUPTED, AVATAR_OFFLINE } pp_avatar_state_t;
typedef struct { uint32_t generation, turn; pp_avatar_state_t state; bool active; } pp_avatar_t;
void pp_avatar_begin(pp_avatar_t *avatar, uint32_t generation, uint32_t turn);
bool pp_avatar_event(pp_avatar_t *avatar, uint32_t generation, uint32_t turn, pp_avatar_state_t state);
void pp_avatar_end(pp_avatar_t *avatar);

typedef enum { AVATAR_LOOK, AVATAR_READ, AVATAR_HELP, AVATAR_WALK, AVATAR_EAT,
               AVATAR_PLAY, AVATAR_SLEEP, AVATAR_GROW } pp_avatar_pose_t;
/* Explicit chat-only visual parameters. Zero initialization preserves the
 * original pet renderer exactly. Values are pixel offsets, never transforms. */
typedef struct {
    bool enabled, blink;
    uint8_t breathe, ear, mouth, dots;
    int8_t gaze, tilt;
} pp_avatar_chat_frame_t;
typedef struct {
    pp_avatar_pose_t pose; pp_avatar_state_t voice; unsigned frame; bool evolved;
    pp_avatar_chat_frame_t chat;
} pp_avatar_frame_t;
typedef void (*pp_pixel_rect_fn)(void *ctx, int x, int y, int w, int h, uint32_t rgb);
typedef struct { pp_pixel_rect_fn rect; void *ctx; } pp_pixel_sink_t;
/* Stateless, shared, no heap/framebuffer/image decoder; native pixel coordinates. */
void pp_avatar_draw(const pp_pixel_sink_t *sink, int x, int y, const pp_avatar_frame_t *frame);
void pp_pet_room_draw(const pp_pixel_sink_t *sink);
