"""Validate and generate the single build/runtime application catalog."""
import json
from pathlib import Path
import re
import sys

def read_catalog(root):
    data=json.loads((root/'apps/catalog.json').read_text(encoding='utf-8'))
    assert data['schema']==1
    apps=data['apps']; assert 1<=len(apps)<=16
    for field in ('id','component','symbol','namespace'):
        assert len({a[field] for a in apps})==len(apps), 'Duplicate '+field
        for a in apps:
            assert re.fullmatch(r'[a-z][a-z0-9_]*',a[field]), 'Unsafe '+field
    for a in apps:
        assert len(a['id'])<24 and len(a['namespace'])<=15
        for field in ('name','version','author'):
            assert isinstance(a[field],str) and 0<len(a[field])<24 and a[field].isascii()
        assert (root/'components'/a['component']/'CMakeLists.txt').is_file()
    return apps

def generate(root,out):
    apps=read_catalog(root); out.mkdir(parents=True,exist_ok=True)
    def write(name,text):
        p=out/name
        if not p.exists() or p.read_text()!=text: p.write_text(text,encoding='utf-8')
    write('passport_catalog.h', '#pragma once\n#include "passport_apps.h"\n#define PP_APP_COUNT '+str(len(apps))+
        '\nextern const pp_app_module_t *const pp_modules[PP_APP_COUNT];\nextern pp_app_t pp_apps[PP_APP_COUNT];\nextern const char *const pp_namespaces[PP_APP_COUNT];\nvoid pp_catalog_init(void);\n')
    c='#include "passport_catalog.h"\n'
    for a in apps: c+='extern const pp_app_module_t '+a['symbol']+';\n'
    c+='const pp_app_module_t *const pp_modules[PP_APP_COUNT]={'+','.join('&'+a['symbol'] for a in apps)+'};\n'
    c+='const char *const pp_namespaces[PP_APP_COUNT]={'+','.join(json.dumps(a['namespace']) for a in apps)+'};\n'
    c+='pp_app_t pp_apps[PP_APP_COUNT];\nvoid pp_catalog_init(void) {\n'
    for i,a in enumerate(apps):
        c+='pp_apps['+str(i)+']=(pp_app_t){'+','.join(json.dumps(a[k]) for k in ('id','name','version','author'))
        c+=', '+', '.join(a['symbol']+'.'+f for f in ('start','stop','focus','key','ctx'))+'};\n'
    c+='}\n'; write('passport_catalog.c',c)
    write('passport_catalog.cmake','set(PP_COMPONENTS '+' '.join(a['component'] for a in apps)+')\n')
    # Fixed-size metrics object makes the second link independent of numbers.
    seed=out/'passport_metrics.h'
    if not seed.exists(): seed.write_text('#define PP_METRICS_INIT {0}\n')
    return apps
if __name__=='__main__':
    root=Path(__file__).resolve().parent.parent
    if len(sys.argv)==1: print('Catalog: PASS ('+str(len(read_catalog(root)))+' custom apps)')
    else: generate(root,Path(sys.argv[1]))
