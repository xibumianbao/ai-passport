#pragma once
#include <stdbool.h>
typedef enum { PP_SCREEN_ON, PP_SCREEN_PAUSED, PP_SCREEN_RUNNING } pp_screen_state_t;
typedef enum { PP_SCREEN_IGNORE, PP_SCREEN_WAKE, PP_SCREEN_PASS, PP_SCREEN_MENU } pp_screen_action_t;
/* Running screen-off preserves focus, session generation and app resources. */
pp_screen_state_t pp_screen_off(bool keep_running, bool app_visible);
pp_screen_action_t pp_screen_input(pp_screen_state_t state, bool menu_key);
