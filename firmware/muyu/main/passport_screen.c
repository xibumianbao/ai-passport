#include "passport_screen.h"
pp_screen_state_t pp_screen_off(bool keep, bool app) { return keep && app ? PP_SCREEN_RUNNING : PP_SCREEN_PAUSED; }
pp_screen_action_t pp_screen_input(pp_screen_state_t state, bool menu)
{
    if (state == PP_SCREEN_PAUSED) return PP_SCREEN_WAKE;
    if (state == PP_SCREEN_RUNNING && menu) return PP_SCREEN_MENU;
    return PP_SCREEN_PASS;
}
