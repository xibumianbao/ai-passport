"""Create one flashable BIN and ZIP from a successful, matching CI download."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

def package(source,output):
    root=Path(__file__).resolve().parent.parent
    fw=source/'firmware'; info=json.loads((fw/'build-info.json').read_text(encoding='utf-8-sig'))
    ci=json.loads((source/'ci-result.json').read_text(encoding='utf-8-sig'))
    assert ci['conclusion']=='success' and ci['headSha']==info['source_commit']
    assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()==ci['headSha']
    assert not subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).strip(), 'Source must be clean'
    assert info['application']=='passport-platform' and info['device_tests']=='NOT RUN'
    image=fw/'FoloToy-AI-Passport-full.bin'; data=image.read_bytes(); digest=hashlib.sha256(data).hexdigest()
    assert digest==info['merged_sha256'] and len(data)==info['merged_bytes']<=0x310000
    subprocess.run([sys.executable,str(root/'tools/verify_firmware.py'),str(fw)],check=True)
    for line in (fw/'SHA256SUMS').read_text().splitlines():
        h,name=line.split(maxsplit=1); assert hashlib.sha256((fw/name.strip()).read_bytes()).hexdigest()==h
    capacity=json.loads((fw/'capacity-report.json').read_text())
    assert capacity['image_bytes']==(fw/'FoloToy-AI-Passport.bin').stat().st_size
    assert capacity['program_free']==0x300000-capacity['image_bytes']
    log=(source/'ci-log.txt').read_text(encoding='utf-8-sig')
    required=['Passport core: PASS','Passport extensions: PASS','Passport UI: PASS','Application scaffold: PASS','Capacity report: PASS','Firmware build: PASS']
    evidence=[line for line in log.splitlines() if any(k in line for k in required) and 'echo ' not in line]
    for key in required: assert any(key in line for line in evidence), 'Missing '+key
    name='ai-passport-platform-v'+info['version']+'-'+info['source_commit'][:7]
    output.mkdir(parents=True,exist_ok=True); target=output/(name+'-full.bin')
    target.write_bytes(data)
    readme=(root/'README.zh_CN.md').read_text(encoding='utf-8')
    instructions='AI Passport '+info['version']+'\n\n'+target.name+'\nOffset: 0x0. Do not erase the whole chip.\nSHA-256: '+digest+'\nCI: '+ci['url']+'\n\nBuild/host: PASS. New device tests: NOT RUN.\n\n'+readme
    zpath=output/(name+'.zip')
    with zipfile.ZipFile(zpath,'w',zipfile.ZIP_DEFLATED) as z:
        z.write(target,target.name); z.writestr('SHA256SUMS',digest+'  '+target.name+'\n')
        z.writestr('README.txt',instructions); z.writestr('test-results.txt','\n'.join(evidence)+'\nDevice tests: NOT RUN\n')
        z.write(root/'LICENSE','LICENSE')
        z.write(root/'assets/fonts/SourceHanSans-OFL.txt','licenses/SourceHanSans-OFL.txt')
        for f in ['build-info.json','capacity-report.json']: z.write(fw/f,f)
        z.write(source/'ci-result.json','ci-result.json')
        for f in ['platform-acceptance.md','platform-acceptance.zh_CN.md','app-integration.md','app-integration.zh_CN.md','community-compatibility.md','community-compatibility.zh_CN.md']:
            z.write(root/'docs'/f,'docs/'+f)
        for f in sorted((source/'preview').glob('passport-*.png')): z.write(f,'preview/'+f.name)
    with zipfile.ZipFile(zpath) as z:
        assert z.testzip() is None and hashlib.sha256(z.read(target.name)).hexdigest()==digest
    result={'result':'PASS','source_commit':ci['headSha'],'bin':str(target.resolve()),'zip':str(zpath.resolve()),'sha256':digest,'bytes':len(data)}
    (source/'package-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False)); return result

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('download',type=Path); p.add_argument('--output',type=Path,required=True); args=p.parse_args()
    package(args.download.resolve(),args.output.resolve())
