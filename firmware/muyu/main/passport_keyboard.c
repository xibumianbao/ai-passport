#include "passport_keyboard.h"
#include <string.h>
/* Independent implementation of a three-button paged keyboard. Every printable
 * ASCII password character is reachable; GO/DEL never overload global Back. */
static const char *const chars[] = {
    "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789!@#$%^&*()-_=+[]{}", "`~\\|;:'\",.<>/?"
};
static const char *const actions[] = {"abc", "ABC", "123", "#+=", "SP", "DEL", "GO", "BACK"};
static char labels[95][2];
void pp_keyboard_init(pp_keyboard_t *kb, unsigned limit)
{
    memset(kb, 0, sizeof(*kb)); kb->limit = limit <= 63 ? limit : 63;
    for (unsigned i=0; i<95; ++i) { labels[i][0]=(char)(32+i); labels[i][1]=0; }
}
unsigned pp_keyboard_count(unsigned page) { return page < 4 ? strlen(chars[page])+8 : 0; }
const char *pp_keyboard_label(unsigned page, unsigned index)
{
    if (page >= 4 || index >= pp_keyboard_count(page)) return "";
    unsigned n=strlen(chars[page]);
    return index < n ? labels[(unsigned char)chars[page][index]-32] : actions[index-n];
}
pp_kb_result_t pp_keyboard_input(pp_keyboard_t *kb, pp_key_t key)
{
    if (key == PP_MENU) return PP_KB_CANCEL;
    unsigned n=pp_keyboard_count(kb->page);
    if (!n) { pp_keyboard_init(kb,63); return PP_KB_EDIT; }
    if (key == PP_UP || key == PP_DOWN) {
        kb->selected=(kb->selected+n+(key == PP_UP ? -1 : 1))%n; return PP_KB_EDIT;
    }
    if (key != PP_OK) return PP_KB_EDIT;
    unsigned count=strlen(chars[kb->page]), action=kb->selected-count;
    if (kb->selected >= count) {
        if (action < 4) { kb->page=action; kb->selected=0; return PP_KB_EDIT; }
        if (action == 5) { size_t len=strlen(kb->value); if (len) kb->value[len-1]=0; return PP_KB_EDIT; }
        if (action == 6) return PP_KB_SUBMIT;
        if (action == 7) return PP_KB_CANCEL;
    }
    size_t len=strlen(kb->value);
    if (len >= kb->limit) return PP_KB_FULL;
    kb->value[len]=kb->selected < count ? chars[kb->page][kb->selected] : ' ';
    kb->value[len+1]=0; return PP_KB_EDIT;
}
bool pp_wifi_credentials_valid(const char *ssid, const char *pass, bool open)
{
    if (!ssid || !pass) return false;
    size_t sn=0,pn=0;
    while (sn<33 && ssid[sn]) { if ((unsigned char)ssid[sn]<32 || (unsigned char)ssid[sn]==127) return false; ++sn; }
    while (pn<64 && pass[pn]) { if ((unsigned char)pass[pn]<32 || (unsigned char)pass[pn]>126) return false; ++pn; }
    return sn>0 && sn<=32 && (open ? pn==0 : pn>=8 && pn<=63);
}
