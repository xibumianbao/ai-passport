"""Scaffold a custom app in the catalog; does not build, push or flash."""
import argparse
import json
from pathlib import Path
import re
from generate_catalog import read_catalog

def scaffold(root,app_id,name):
    assert re.fullmatch(r'[a-z][a-z0-9_]{0,10}',app_id), 'Use 1-11 lowercase letters/digits/underscores'
    assert name.isascii() and 0<len(name)<24 and all(ord(c)>=32 for c in name)
    apps=read_catalog(root); assert len(apps)<16
    assert app_id not in {a['id'] for a in apps}
    component='pp_app_'+app_id; folder=root/'components'/component
    assert not folder.exists(), 'Never overwrite an existing component'
    app={'id':app_id,'name':name,'version':'0.1.0','author':'AI Passport','component':component,
         'symbol':'pp_'+app_id+'_module','namespace':'app_'+app_id}
    code='''#include "passport_apps.h"
#include "lvgl.h"
static unsigned s_count;
static lv_obj_t *s_label;
static bool start(void *ctx) { (void)ctx; return true; }
static void stop(void *ctx) { (void)ctx; }
static void key(void *ctx,pp_key_t key) { (void)ctx; (void)key; ++s_count; }
static void create_ui(lv_obj_t *parent)
{
    s_label=lv_label_create(parent); lv_obj_set_pos(s_label,16,40); lv_obj_set_width(s_label,208);
}
static void render_ui(void) { lv_label_set_text_fmt(s_label,"Key events: %u",s_count); }
const pp_app_module_t SYMBOL={start,stop,NULL,key,NULL,create_ui,render_ui};
'''.replace('SYMBOL',app['symbol'])
    folder.mkdir(parents=True)
    (folder/(component+'.c')).write_text(code,encoding='utf-8')
    (folder/'CMakeLists.txt').write_text('idf_component_register(SRCS "'+component+'.c" INCLUDE_DIRS "../../main" REQUIRES bsp)\n')
    (root/'apps/catalog.json').write_text(json.dumps({'schema':1,'apps':apps+[app]},indent=2)+'\n')
    return [str(folder.relative_to(root)/(component+'.c')),str(folder.relative_to(root)/'CMakeLists.txt'),'apps/catalog.json']
if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('id'); p.add_argument('--name',required=True); args=p.parse_args()
    print('\n'.join(scaffold(Path(__file__).resolve().parent.parent,args.id,args.name)))
