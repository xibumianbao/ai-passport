"""Embed and verify per-app linked contributions; never apportion shared SDKs."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from generate_catalog import read_catalog

def analyze(raw,apps,image_bytes):
    assert raw['target']=='esp32c3'
    result=[]
    for app in apps:
        flash=ram=0
        for mem,info in raw['memory_types'].items():
            for section,sec in info['sections'].items():
                for archive,arc in sec['archives'].items():
                    if Path(archive).name!='lib'+app['component']+'.a': continue
                    size=arc['size']; assert size>=0
                    # RAM data/text have a Flash load image; BSS/noinit do not.
                    if 'bss' not in section and 'noinit' not in section: flash+=size
                    if 'flash' not in mem.lower(): ram+=size
        assert flash>0, 'Missing linked application archive: '+app['id']
        result.append({'id':app['id'],'name':app['name'],'namespace':app['namespace'],
                       'flash_bytes':flash,'static_ram_bytes':ram})
    owned=sum(a['flash_bytes'] for a in result)
    assert 0<owned<=image_bytes<=0x300000
    return {'schema':1,'image_bytes':image_bytes,'program_limit':0x300000,'program_free':0x300000-image_bytes,
            'shared_and_image_overhead_bytes':image_bytes-owned,'apps':result,
            'accounting':'Linked app archives only. Shared SDK/UI/audio worker and image padding counted once. Static RAM excludes task stacks, heap and DMA.',
            'external_asset_bytes':0}

def initializer(report):
    a=report['apps']
    return '#define PP_METRICS_INIT {1, '+str(report['image_bytes'])+', {'+','.join(str(x['flash_bytes']) for x in a)+'}, {'+','.join(str(x['static_ram_bytes']) for x in a)+'}}\n'

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('build',type=Path); parser.add_argument('--verify',action='store_true'); args=parser.parse_args()
    root=Path(__file__).resolve().parent.parent; b=args.build
    cmd=[sys.executable,'-m','esp_idf_size','--format','raw',str(b/'FoloToy-AI-Passport.map')]
    raw=json.loads(subprocess.check_output(cmd,text=True)); report=analyze(raw,read_catalog(root),(b/'FoloToy-AI-Passport.bin').stat().st_size)
    h=b/'passport_generated/passport_metrics.h'; expected=initializer(report)
    if args.verify:
        assert h.read_text()==expected, 'Capacity changed after second link; refuse stale device metrics'
        (b/'capacity-report.json').write_text(json.dumps(report,indent=2)+'\n')
        # Sanitized summary only: raw map contains local build paths.
        print('Capacity report: PASS (final image and application archive sizes match embedded metrics)')
    else: h.write_text(expected)
if __name__=='__main__': main()
