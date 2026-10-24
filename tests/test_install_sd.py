from pathlib import Path
import importlib.util, tempfile, unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('install_sd',ROOT/'tools/install_sd.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Installer(unittest.TestCase):
    def target(self,root):
        sd=root/'sd';(sd/'bootloader').mkdir(parents=True);(sd/'atmosphere').mkdir()
        (sd/'atmosphere/package3').write_bytes(b'untouched-fixture')
        (sd/'bootloader/hekate_ipl.ini').write_bytes(b'[config]\r\nautoboot=0\r\n[Sistema Original]\r\nstock=1\r\nemummc_force_disable=1\r\n[Sistema Hack]\r\npkg3=atmosphere/package3\r\nemummcforce=1\r\n')
        return sd

    def snapshot(self,sd):return {p.relative_to(sd).as_posix():p.read_bytes() for p in sd.rglob('*') if p.is_file()}

    def test_plan_is_read_only_and_install_backs_up(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);sd=self.target(root);before=self.snapshot(sd)
            actual,files,old=m.plan(sd,'audio')
            self.assertEqual(before,self.snapshot(sd))
            m.apply(actual,files,old,root/'backup')
            self.assertTrue((root/'backup/BACKUP.json').is_file())
            for name,data in before.items():self.assertEqual((sd/name).read_bytes(),data)
            for name,data in files.items():self.assertEqual((sd/name).read_bytes(),data)
            self.assertIn(b'yoshi_sound.wav',(sd/'config/JheraldBootSound/config.ini').read_bytes())

    def test_existing_selection_is_preserved(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);sd=self.target(root);config=sd/'config/JheraldBootSound';config.mkdir(parents=True)
            (config/'config.ini').write_bytes(b'audio_file=mine.wav\n');(config/'startup.wav').write_bytes(b'keep-fallback')
            _,files,_=m.plan(sd,'audio')
            self.assertNotIn('config/JheraldBootSound/config.ini',files)
            self.assertNotIn('config/JheraldBootSound/startup.wav',files)
            _,files,_=m.plan(sd,'audio',replace_config=True)
            self.assertIn('config/JheraldBootSound/config.ini',files)

    def test_changed_target_aborts_before_backup_or_writes(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);sd=self.target(root);actual,files,old=m.plan(sd,'audio')
            p=sd/(m.MODULE+'/exefs.nsp');p.parent.mkdir(parents=True);p.write_bytes(b'changed')
            before=self.snapshot(sd)
            with self.assertRaises(ValueError):m.apply(actual,files,old,root/'backup')
            self.assertEqual(before,self.snapshot(sd));self.assertFalse((root/'backup').exists())

    def test_backup_inside_sd_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            sd=self.target(Path(t));actual,files,old=m.plan(sd,'audio');before=self.snapshot(sd)
            with self.assertRaises(ValueError):m.apply(actual,files,old,sd/'backup')
            self.assertEqual(before,self.snapshot(sd))

    def test_logo_preserves_other_entries_and_makes_verified_backup(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);sd=self.target(root);before=(sd/'bootloader/hekate_ipl.ini').read_bytes()
            actual,files,old=m.plan(sd,'logo');after=files['bootloader/hekate_ipl.ini']
            self.assertEqual(after.replace(b'logopath=bootloader/res/jherald_bootlogo.bmp\r\n',b''),before)
            m.apply(actual,files,old,root/'backup')
            self.assertEqual((root/'backup/bootloader/hekate_ipl.ini').read_bytes(),before)

    def test_boot_entry_detection_preserves_custom_names(self):
        with tempfile.TemporaryDirectory() as t:
            sd=self.target(Path(t));p=sd/'bootloader/hekate_ipl.ini'
            p.write_bytes(p.read_bytes().replace(b'Sistema Original',b'Mi OFW').replace(b'Sistema Hack',b'Mi CFW'))
            before=p.read_bytes()
            self.assertEqual(m.boot_entry(sd,'sysMMC'),'Mi OFW')
            self.assertEqual(m.boot_entry(sd,'emuMMC'),'Mi CFW')
            _,files,_=m.plan(sd,'logo')
            self.assertEqual(files['bootloader/hekate_ipl.ini'].replace(b'logopath=bootloader/res/jherald_bootlogo.bmp\r\n',b''),before)
            self.assertEqual(p.read_bytes(),before)

    def test_ambiguous_or_missing_boot_entry_requires_explicit_name(self):
        with tempfile.TemporaryDirectory() as t:
            sd=self.target(Path(t));p=sd/'bootloader/hekate_ipl.ini'
            p.write_bytes(p.read_bytes()+b'[Otra emuMMC]\r\nemummcforce=1\r\n')
            with self.assertRaises(ValueError):m.boot_entry(sd,'emuMMC')
            with self.assertRaises(ValueError):m.plan(sd,'logo')
            _,files,_=m.plan(sd,'logo',entry='Sistema Hack')
            self.assertIn(b'[Otra emuMMC]',files['bootloader/hekate_ipl.ini'])

    def test_plain_fusee_entry_is_detected(self):
        with tempfile.TemporaryDirectory() as t:
            sd=self.target(Path(t));p=sd/'bootloader/hekate_ipl.ini'
            p.write_bytes(p.read_bytes().replace(b'pkg3=atmosphere/package3\r\nemummcforce=1',b'payload=bootloader/payloads/fusee.bin'))
            self.assertEqual(m.boot_entry(sd,'emuMMC'),'Sistema Hack')

    def test_logo_replaces_one_option_and_rejects_unknown_entry(self):
        before=b'[Hack]\nlogopath=old.bmp\npkg3=atmosphere/package3\n[Original]\nstock=1\n'
        after=m.logo_config(before,'Hack');self.assertEqual(after.count(b'logopath='),1)
        self.assertTrue(after.endswith(b'[Original]\nstock=1\n'))
        for entry in ('missing','bad\nname'):
            with self.assertRaises(ValueError):m.logo_config(before,entry)
        with self.assertRaises(ValueError):m.logo_config(before+b'[Hack]\nx=1\n','Hack')

if __name__=='__main__':unittest.main()
