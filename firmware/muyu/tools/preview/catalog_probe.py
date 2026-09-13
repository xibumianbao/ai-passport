"""Exercise the real scaffold and registry without adding a shipping app."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools'))
from generate_catalog import read_catalog,generate
from new_app import scaffold
out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
# This independent Muyu + generated app fixture stays buildable as shipping
# apps grow hardware-specific dependencies; each new app supplies its own tests.
apps=[{'id':'muyu','name':'Muyu','version':'0.3.0','author':'AI Passport',
       'component':'pp_app_muyu','symbol':'pp_muyu_module','namespace':'app_muyu'}]
(out/'apps').mkdir(exist_ok=True)
(out/'apps/catalog.json').write_text(json.dumps({'schema':1,'apps':apps}))
for a in apps:
    p=out/'components'/a['component']; p.mkdir(parents=True,exist_ok=True); (p/'CMakeLists.txt').touch()
if not (out/'components/pp_app_probe').exists(): scaffold(out,'probe','Build Probe')
else:
    # CMake reconfigure: retain the generated fixture without overwriting source.
    app={'id':'probe','name':'Build Probe','version':'0.1.0','author':'AI Passport','component':'pp_app_probe','symbol':'pp_probe_module','namespace':'app_probe'}
    (out/'apps/catalog.json').write_text(json.dumps({'schema':1,'apps':apps+[app]}))
generate(out,out/'generated')
