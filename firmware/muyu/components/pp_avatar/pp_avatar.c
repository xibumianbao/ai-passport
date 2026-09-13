#include "pp_avatar.h"
void pp_avatar_begin(pp_avatar_t *a, uint32_t generation, uint32_t turn)
{
    *a=(pp_avatar_t){.generation=generation,.turn=turn,.state=AVATAR_IDLE,.active=true};
}
bool pp_avatar_event(pp_avatar_t *a, uint32_t generation, uint32_t turn, pp_avatar_state_t state)
{
    if (!a->active || a->generation!=generation || a->turn!=turn ||
        state<AVATAR_IDLE || state>AVATAR_OFFLINE) return false;
    a->state=state;
    if(state==AVATAR_INTERRUPTED || state==AVATAR_OFFLINE) a->active=false;
    return true;
}
void pp_avatar_end(pp_avatar_t *a) { a->active=false; a->state=AVATAR_IDLE; }
