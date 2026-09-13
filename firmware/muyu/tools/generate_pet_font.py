"""Regenerate the shared OFL Yaya font subset from both independent UIs."""
import argparse
from pathlib import Path
import re
import subprocess
import shutil

root=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(); p.add_argument('font',type=Path); args=p.parse_args()
source='\n'.join((root/path).read_text(encoding='utf-8') for path in (
    'components/pp_app_pet/pp_pet_ui.c','components/pp_app_xiaozhi/pp_xiaozhi_ui.c'))
symbols=''.join(sorted(set(re.findall(r'[\u3000-\u9fff\uff00-\uffef★]',source))))
out=root/'main/font_pet_14.c'
subprocess.run([shutil.which('npx') or shutil.which('npx.cmd'),'--yes','--package=lv_font_conv@1.5.3',
    'lv_font_conv','--font',str(args.font.resolve()),'--size','14','--bpp','2','--format','lvgl',
    '--no-compress','--range','0x20-0x7E','--symbols',symbols,'-o',str(out)],check=True)
text=out.read_text(encoding='utf-8')
text=re.sub(r' \* Opts: .*', ' * Source: Source Han Sans SC Medium, OFL; tools/generate_pet_font.py',text)
out.write_text(text.replace('#include "lvgl/lvgl.h"','#include "lvgl.h"').rstrip()+'\n',encoding='utf-8')
print('Pet font: generated ASCII +',len(symbols),'UI glyphs (14 px, 2 bpp)')
