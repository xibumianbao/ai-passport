#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PP_PET_SAVE_SIZE 48
#define PP_PET_CYCLE_MS 60000u
#define PP_PET_MAX_XP 56u
typedef enum { PET_LIVE, PET_EAT, PET_PLAY, PET_REST, PET_GROW } pp_pet_action_t;
typedef enum { PET_FEED, PET_TOGETHER, PET_SLEEP } pp_pet_choice_t;
typedef enum { PET_QUIET, PET_REWARD, PET_FED, PET_FULL, PET_FREE_FOOD, PET_LEVEL,
               PET_EVOLVED, PET_RESTED, PET_STORAGE_ERROR } pp_pet_notice_t;
typedef struct {
    uint32_t id, sequence, cycles, cycle_ms;
    uint16_t coins, xp;
    uint8_t fullness, energy;
    bool resting;
} pp_pet_save_t;
typedef bool (*pp_pet_write_fn)(void *ctx, const uint8_t bytes[PP_PET_SAVE_SIZE]);
typedef struct {
    pp_pet_save_t save;
    pp_pet_write_fn write;
    void *write_ctx;
    pp_pet_action_t action;
    pp_pet_choice_t choice;
    pp_pet_notice_t notice;
    uint32_t action_ms, clock_ms, notice_ms, rest_ms;
    bool storage_error, dirty, selection_locked;
} pp_pet_t;

void pp_pet_defaults(pp_pet_save_t *save, uint32_t id);
unsigned pp_pet_level(const pp_pet_save_t *save);
unsigned pp_pet_progress(const pp_pet_save_t *save);
void pp_pet_encode(const pp_pet_save_t *save, uint8_t bytes[PP_PET_SAVE_SIZE]);
bool pp_pet_decode(pp_pet_save_t *save, const uint8_t *bytes, size_t length);
/* Select newest valid record. -1 means neither is valid; never erase either. */
int pp_pet_choose_save(pp_pet_save_t *save, const uint8_t *a, size_t na,
                       const uint8_t *b, size_t nb);
void pp_pet_init(pp_pet_t *pet, const pp_pet_save_t *save, pp_pet_write_fn write, void *ctx);
bool pp_pet_checkpoint(pp_pet_t *pet);
/* Elapsed active time only. Hidden/menu/dialogue time is supplied as zero. */
void pp_pet_tick(pp_pet_t *pet, uint32_t elapsed_ms);
void pp_pet_select(pp_pet_t *pet, int direction);
void pp_pet_press(pp_pet_t *pet);
