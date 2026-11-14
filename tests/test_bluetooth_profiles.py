from pathlib import Path
import hashlib,importlib.util,json,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1];BT=ROOT/'components/bluetooth-sync'
spec=importlib.util.spec_from_file_location('profiles_prepare',BT/'prepare.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

class Profiles(unittest.TestCase):
    def fixture(self,root):
        sd=root/'sd'
        files={'bootloader/hekate_ipl.ini':b'[config]\nautoboot=0\n[Original]\nstock=1\nemummc_force_disable=1\n[Hack]\npkg3=atmosphere/package3\nemummcforce=1\n',
               'emuMMC/emummc.ini':b'[emummc]\nenabled=1\n',
               'atmosphere/config/system_settings.ini':b'[atmosphere]\nenable_external_bluetooth_db=u8!0x1\n',
               'atmosphere/bluetooth_devices.db':bytes(8),'switch/prod.keys':b'synthetic-only',
               'bootloader/update.bin':(BT/'test-assets/hekate-6.5.4.bin').read_bytes()}
        for name,data in files.items():dest=sd/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        return sd

    def test_both_official_loaders_and_legacy_continuation_remain_separate(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd=self.fixture(root);output=root/'out';old_out=p.OUT;old_root=p.ROOT
            p.OUT=output;p.ROOT=root/'project';(p.ROOT/'include').mkdir(parents=True)
            try:
                legacy=p.prepare(sd,'Original','Hack',4096,100000,'23.0.1')
                self.assertEqual(legacy['running_hekate_version'],'6.5.4');self.assertEqual(legacy['compatibility_status'],'legacy')
                cont=sd/'switch/bt-autosync-v1/hekate-continue.bin';cont.parent.mkdir();cont.write_bytes((sd/'bootloader/update.bin').read_bytes())
                (sd/'bootloader/update.bin').write_bytes((BT/'test-assets/hekate-6.5.5.bin').read_bytes())
                candidate=p.prepare(sd,'Original','Hack',4096,100000,'23.0.1',continuation='existing')
                self.assertEqual(candidate['running_hekate_version'],'6.5.5');self.assertEqual(candidate['continuation_version'],'6.5.4')
                self.assertEqual(candidate['compatibility_status'],'candidate')
                self.assertEqual(cont.read_bytes(),(BT/'test-assets/hekate-6.5.4.bin').read_bytes())
                (sd/'bootloader/update.bin').write_bytes(b'unknown loader')
                with self.assertRaises(ValueError):p.prepare(sd,'Original','Hack',4096,100000,'23.0.1')
            finally:p.OUT=old_out;p.ROOT=old_root

    def test_extended_controllers_require_enabled_missioncontrol(self):
        with tempfile.TemporaryDirectory() as temp:
            sd=self.fixture(Path(temp))
            with self.assertRaisesRegex(ValueError,'MissionControl'):p.prepare(sd,'Original','Hack',4096,100000,'23.0.1','missioncontrol')
            with self.assertRaises(ValueError):p.prepare(sd,'Original','Hack',4096,100000,'23.5.0')
            with self.assertRaises(ValueError):p.prepare(sd,'Original','Hack',4096,100000,'23.0.1','all-bluetooth')

if __name__=='__main__':unittest.main()
