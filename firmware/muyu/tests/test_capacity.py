import copy
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from build_capacity import analyze, initializer, check_chat_budget
from generate_catalog import read_catalog, generate
import tempfile
import json

class CapacityTest(unittest.TestCase):
    def setUp(self):
        self.apps=[{'id':'a','name':'A','namespace':'app_a','component':'pp_app_a'}]
        self.raw={'target':'esp32c3','memory_types':{
            'Flash Code':{'sections':{'.flash.text':{'archives':{'esp-idf/pp_app_a/libpp_app_a.a':{'size':100}}}}},
            'Flash Data':{'sections':{'.flash.rodata':{'archives':{'esp-idf/pp_app_a/libpp_app_a.a':{'size':20},'libshared.a':{'size':200}}}}},
            'DIRAM':{'sections':{'.dram0.data':{'archives':{'esp-idf/pp_app_a/libpp_app_a.a':{'size':10}}},
                                 '.dram0.bss':{'archives':{'esp-idf/pp_app_a/libpp_app_a.a':{'size':50}}}}}}}
    def test_no_double_count_or_bss_flash(self):
        r=analyze(self.raw,self.apps,400)
        self.assertEqual(r['apps'][0]['flash_bytes'],130)
        self.assertEqual(r['apps'][0]['static_ram_bytes'],60)
        self.assertEqual(r['shared_and_image_overhead_bytes'],270)
        self.assertEqual(r['program_free'],0x300000-400)
        self.assertIn('{130}',initializer(r))
    def test_absent_archive_is_not_zero_usage(self):
        self.apps[0]['component']='missing'
        with self.assertRaises(AssertionError): analyze(self.raw,self.apps,400)
    def test_shared_avatar_counts_once(self):
        self.raw['memory_types']['Flash Code']['sections']['.flash.text']['archives']['libpp_avatar.a']={'size':80}
        self.raw['memory_types']['DIRAM']['sections']['.dram0.bss']['archives']['libpp_avatar.a']={'size':4}
        r=analyze(self.raw,self.apps,400)
        self.assertEqual(r['shared_components'],[{'component':'pp_avatar','flash_bytes':80,'static_ram_bytes':4}])
        self.assertEqual(r['apps'][0]['flash_bytes'],130)
        self.assertEqual(r['shared_and_image_overhead_bytes'],270)
    def test_overflow_is_rejected(self):
        with self.assertRaises(AssertionError): analyze(self.raw,self.apps,0x300001)
    def test_chat_release_budget_rejects_growth_and_missing_component(self):
        root=Path(__file__).resolve().parents[1]
        budget=json.loads((root/'apps/chat-motion-budget.json').read_text())
        report={'image_bytes':1818192,'apps':[{'id':'xiaozhi','static_ram_bytes':36}],
                'shared_components':[{'component':'pp_avatar','static_ram_bytes':4309},
                                     {'component':'pp_voice','static_ram_bytes':394}]}
        check_chat_budget(report,'0.6.1',budget)
        self.assertEqual(report['chat_motion_budget']['static_ram_growth_bytes'],256)
        report['image_bytes']+=1
        with self.assertRaises(AssertionError): check_chat_budget(report,'0.6.1',budget)
        report['image_bytes']-=1
        report['apps'][0]['static_ram_bytes']+=1
        with self.assertRaises(AssertionError): check_chat_budget(report,'0.6.1',budget)
        report['apps'].clear()
        with self.assertRaises(AssertionError): check_chat_budget(report,'0.6.1',budget)
    def test_catalog_registration_and_duplicates(self):
        root=Path(__file__).resolve().parents[1]
        apps=read_catalog(root); self.assertGreaterEqual(len(apps),1)
        (root/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=root/'build') as temp:
            p=Path(temp); generate(root,p)
            assert p.resolve().is_relative_to((root/'build').resolve())
            self.assertIn(apps[0]['symbol'],(p/'passport_catalog.c').read_text())
            (p/'apps').mkdir(); (p/'components'/apps[0]['component']).mkdir(parents=True)
            (p/'components'/apps[0]['component']/'CMakeLists.txt').touch()
            (p/'apps/catalog.json').write_text(json.dumps({'schema':1,'apps':[apps[0],copy.deepcopy(apps[0])]}))
            with self.assertRaises(AssertionError): read_catalog(p)
if __name__=='__main__': unittest.main()
