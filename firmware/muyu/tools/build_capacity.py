"""Embed and verify per-app linked contributions; never apportion shared SDKs."""
import argparse
import json
import os
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
    shared=[]
    for component in ('pp_avatar',):
        flash=ram=0
        for mem,info in raw['memory_types'].items():
            for section,sec in info['sections'].items():
                for archive,arc in sec['archives'].items():
                    if Path(archive).name!='lib'+component+'.a': continue
                    if 'bss' not in section and 'noinit' not in section: flash+=arc['size']
                    if 'flash' not in mem.lower(): ram+=arc['size']
        if flash: shared.append({'component':component,'flash_bytes':flash,'static_ram_bytes':ram})
    return {'schema':1,'image_bytes':image_bytes,'program_limit':0x300000,'program_free':0x300000-image_bytes,
            'shared_and_image_overhead_bytes':image_bytes-owned,'apps':result,
            'shared_components':shared,
            'accounting':'Linked app archives only. Shared SDK/UI/audio worker and image padding counted once. Static RAM excludes task stacks, heap and DMA.',
            'external_asset_bytes':0}

def initializer(report):
    a=report['apps']
    return '#define PP_METRICS_INIT {1, '+str(report['image_bytes'])+', {'+','.join(str(x['flash_bytes']) for x in a)+'}, {'+','.join(str(x['static_ram_bytes']) for x in a)+'}}\n'

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('build',type=Path); parser.add_argument('--verify',action='store_true'); args=parser.parse_args()
    root=Path(__file__).resolve().parent.parent; b=args.build
    cmd=[sys.executable,'-m','esp_idf_size','--format','raw',str(b/'FoloToy-AI-Passport.map')]
    # IDF 5.5.3 ships the 1.x CLI, whose default is legacy (no raw format).
    # Select its documented NG implementation; 2.x already uses NG by default.
    env=dict(os.environ,ESP_IDF_SIZE_NG='1')
    raw=json.loads(subprocess.check_output(cmd,text=True,env=env)); report=analyze(raw,read_catalog(root),(b/'FoloToy-AI-Passport.bin').stat().st_size)
    h=b/'passport_generated/passport_metrics.h'; expected=initializer(report)
    if args.verify:
        assert h.read_text()==expected, 'Capacity changed after second link; refuse stale device metrics'
        if any(a['id']=='pet' for a in report['apps']):
            pet=next(a for a in report['apps'] if a['id']=='pet')
            avatar=next(a for a in report['shared_components'] if a['component']=='pp_avatar')
            assert pet['flash_bytes']<=128*1024, 'Pet exceeds its small-app Flash budget'
            assert avatar['flash_bytes']<=48*1024, 'Shared avatar exceeds its Flash budget'
            assert report['program_free']>=1280*1024, 'Preserve at least 1.25 MiB for future voice work'
            report['future_voice_reserve_min_bytes']=1280*1024
            print('Pet capacity: PASS (pet <=128 KiB, avatar <=48 KiB; >=1.25 MiB future headroom)')
        (b/'capacity-report.json').write_text(json.dumps(report,indent=2)+'\n')
        # Sanitized summary only: raw map contains local build paths.
        print('Capacity report: PASS (final image and application archive sizes match embedded metrics)')
    else: h.write_text(expected)
if __name__=='__main__': main()
