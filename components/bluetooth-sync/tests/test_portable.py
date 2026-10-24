"""Synthetic per-SD preparation. No actual console/SD/key access."""
from pathlib import Path
import hashlib,json,struct,sys,tempfile,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import prepare as p
import deploy_portable as d
from deploy_portable import empty_history,verify_keys

class Portable(unittest.TestCase):
    @staticmethod
    def boot_fixture(nl,last_hack=False):
        config=[b'[config]',b'autoboot=2',b'autoboot_list=0',b'bootwait=3']
        original=[b'[Stock OFW]',b'stock=1',b'emummc_force_disable=1']
        hack=[b'[CFW emuMMC]',b'fss0=atmosphere/package3',b'emummcforce=1',b'icon=custom.bmp']
        another=[b'[Another]',b'payload=unchanged.bin']
        return nl.join(config+original+(another+hack if last_hack else hack+another)+[b''])

    def test_named_entries_and_preservation(self):
        for nl in (b'\n',b'\r\n'):
            for last_hack in (False,True):
                with self.subTest(newline=nl,last_hack=last_hack):
                    before=self.boot_fixture(nl,last_hack)
                    after,inner=p.boot_configs(before,'Stock OFW','CFW emuMMC')
                    self.assertEqual(after,before)
                    self.assertEqual(inner,b'')
                    for name in p.parse(before).sections():
                        self.assertEqual(p.section_bytes(before,name),p.section_bytes(after,name))
                    old_config=p.parse(before)
                    new_config=p.parse(after)
                    self.assertEqual(new_config.sections(),old_config.sections())
                    self.assertEqual(dict(new_config['config']),dict(old_config['config']))
                    self.assertEqual(new_config['config']['autoboot'],'2')
                    self.assertEqual(new_config['config']['autoboot_list'],'0')
                    self.assertNotIn(p.MANUAL_ENTRY,new_config)

    def test_existing_fusee_route_preserved_without_sync_loop(self):
        for nl in (b'\n',b'\r\n'):
            before=self.boot_fixture(nl).replace(b'fss0=atmosphere/package3'+nl+b'emummcforce=1',b'payload=bootloader/payloads/fusee.bin')
            after,inner=p.boot_configs(before,'Stock OFW','CFW emuMMC')
            self.assertEqual(after,before)
            self.assertEqual(p.section_bytes(after,'CFW emuMMC'),p.section_bytes(before,'CFW emuMMC'))
            self.assertEqual(inner,b'')
            self.assertNotIn(p.MANUAL_ENTRY,p.parse(after))
            for bad in (before.replace(b'fusee.bin',b'bt-autosync-v1.bin'),before.replace(b'fusee.bin',b'other.bin'),before.replace(b'icon=custom.bmp',b'icon=custom.bmp'+nl+b'emummc_force_disable=1')):
                with self.assertRaises(ValueError):p.boot_configs(bad,'Stock OFW','CFW emuMMC')

    def test_unsafe_or_duplicate_boot_entries_are_rejected(self):
        for nl in (b'\n',b'\r\n'):
            before=self.boot_fixture(nl)
            cases={
                'manual already installed':before+nl.join([b'[Sincronizar Bluetooth una vez]',b'payload=other.bin',b'']),
                'previous automatic hook':before.replace(b'[CFW emuMMC]'+nl,b'[CFW emuMMC]'+nl+b'payload=bootloader/payloads/bt-autosync-v1.bin'+nl),
                'Hack ID already set':before.replace(b'[CFW emuMMC]'+nl,b'[CFW emuMMC]'+nl+b'id=EXISTING'+nl),
                'BTREAL collision':before.replace(b'[Another]',b'id=BTREAL'+nl+b'[Another]'),
                'Original is not stock':before.replace(b'stock=1',b'stock=0'),
                'Hack does not force emuMMC':before.replace(b'emummcforce=1',b'emummcforce=0'),
                'missing final newline':before[:-len(nl)],
                'external list autoboot':before.replace(b'autoboot_list=0',b'autoboot_list=1'),
                'out of range autoboot could select new manual entry':before.replace(b'autoboot=2',b'autoboot=4'),
                'negative autoboot':before.replace(b'autoboot=2',b'autoboot=-1'),
            }
            for label,bad in cases.items():
                with self.subTest(newline=nl,case=label):
                    with self.assertRaises(ValueError):p.boot_configs(bad,'Stock OFW','CFW emuMMC')
            with self.assertRaises(ValueError):p.boot_configs(before,'Bad\nName','CFW emuMMC')

    @staticmethod
    def backup_fixture(root):
        sd=root/'synthetic-sd';sd.mkdir()
        files={
            'bootloader/hekate_ipl.ini':Portable.boot_fixture(b'\n'),
            'emuMMC/emummc.ini':b'[emummc]\nenabled=1\nsector=0x200000\n',
            'atmosphere/config/system_settings.ini':b'[atmosphere]\nenable_external_bluetooth_db=u8!0x1\n',
            'atmosphere/bluetooth_devices.db':b'synthetic destination pairing backup\x00\xff',
        }
        for name,data in files.items():
            path=sd/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        # This is a synthetic sentinel, never actual console key material.
        private=sd/'switch/prod.keys';private.parent.mkdir();private.write_bytes(b'do not copy this synthetic sentinel')
        plan={'sd':str(sd.resolve()),'before_sha256':d.sha(files['bootloader/hekate_ipl.ini']),
              'protected':{name:d.sha(data) for name,data in files.items() if name!='bootloader/hekate_ipl.ini'},
              'original_entry':'Stock OFW','hack_entry':'CFW emuMMC','deletions':[]}
        return sd,files,plan

    def test_destination_backup_copies_pairing_and_configuration(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,files,plan=self.backup_fixture(root);backup=root/'backup'
            manifest=d.backup_target(sd,backup,plan)
            for name,data in files.items():
                self.assertEqual((backup/name).read_bytes(),data)
                self.assertEqual((sd/name).read_bytes(),data)
            self.assertEqual((backup/'hekate_ipl.before.ini').read_bytes(),files['bootloader/hekate_ipl.ini'])
            self.assertEqual(json.loads((backup/'installation-plan.json').read_text()),plan)
            self.assertEqual(json.loads((backup/'BACKUP-SHA256.json').read_text()),manifest)
            self.assertEqual(set(manifest),set(files)|{'hekate_ipl.before.ini','installation-plan.json'})
            for name,digest in manifest.items():self.assertEqual(d.sha((backup/name).read_bytes()),digest)
            self.assertFalse((backup/'switch/prod.keys').exists())

    def test_destination_backup_rejects_existing_or_inside_sd_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,files,plan=self.backup_fixture(root)
            existing=root/'already-exists';existing.mkdir();sentinel=existing/'keep.txt';sentinel.write_bytes(b'keep')
            for backup in (existing,sd/'backup',sd):
                with self.subTest(backup=backup):
                    with self.assertRaises(ValueError):d.backup_target(sd,backup,plan)
            self.assertEqual(sentinel.read_bytes(),b'keep')
            self.assertFalse((sd/'backup').exists())
            for name,data in files.items():self.assertEqual((sd/name).read_bytes(),data)

    def test_destination_backup_rejects_changed_sources(self):
        for changed in ('bootloader/hekate_ipl.ini','emuMMC/emummc.ini',
                        'atmosphere/config/system_settings.ini','atmosphere/bluetooth_devices.db'):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as temp:
                root=Path(temp);sd,files,plan=self.backup_fixture(root)
                (sd/changed).write_bytes(b'changed since plan')
                snapshot={name:(sd/name).read_bytes() for name in files}
                with self.assertRaises(ValueError):d.backup_target(sd,root/'backup',plan)
                self.assertFalse((root/'backup').exists())
                for name,data in snapshot.items():self.assertEqual((sd/name).read_bytes(),data)

    def test_empty_initial_history_no_captures(self):
        cfg=hashlib.sha256(b'synthetic target').digest();state=empty_history(cfg)
        self.assertEqual(len(state),96);self.assertEqual(state[:8],b'BTASYN1\0');self.assertEqual(struct.unpack_from('<II',state,8),(1,0))
        self.assertEqual(state[16:48],cfg);self.assertEqual(state[-32:],hashlib.sha256(state[:-32]).digest())
    def test_required_key_validation_synthetic(self):
        good=b'bis_key_02 = '+b'12'*32+b'\nsave_mac_key = '+b'34'*16+b'\n'
        verify_keys(good)
        for bad in [good+good,b'',good.replace(b'12',b'00'),good[:-4],good.replace(b'34',b'00')]:
            with self.assertRaises(ValueError):verify_keys(bad)

def prepare_synthetic():
    """Generate local test build bindings only, never a distributable console build."""
    original=p.OUT;p.OUT.mkdir(exist_ok=True)
    sd=p.OUT/'synthetic-target';sd.mkdir(exist_ok=True)
    files={'bootloader/hekate_ipl.ini':b'[config]\nautoboot=0\n[Stock OFW]\nstock=1\nemummc_force_disable=1\n[CFW emuMMC]\npkg3=atmosphere/package3\nemummcforce=1\n',
           'emuMMC/emummc.ini':b'[emummc]\nenabled=1\nsector=0x200000\npath=emuMMC/RAW1\n',
           'atmosphere/config/system_settings.ini':b'[atmosphere]\nenable_external_bluetooth_db = u8!0x1\n',
           'atmosphere/bluetooth_devices.db':bytes(8),
           'switch/prod.keys':b'bis_key_02 = '+b'12'*32+b'\nsave_mac_key = '+b'34'*16+b'\n',
           'bootloader/update.bin':(p.ROOT/'test-assets/hekate-6.5.4.bin').read_bytes()}
    for name,data in files.items():
        f=sd/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(data)
    p.prepare(sd.resolve(),'Stock OFW','CFW emuMMC',4096,100000,'23.0.1')
    after=(p.OUT/'hekate_ipl.after.ini').read_bytes()
    assert after==files['bootloader/hekate_ipl.ini']
    assert 'payload' not in p.parse(after)['CFW emuMMC']
    assert p.MANUAL_ENTRY not in p.parse(after)
    assert p.OUT==original
    print('SYNTHETIC bindings prepared: not for installation on a real console')

if __name__=='__main__':
    if '--prepare-synthetic' in sys.argv:prepare_synthetic()
    else:unittest.main(verbosity=2)
