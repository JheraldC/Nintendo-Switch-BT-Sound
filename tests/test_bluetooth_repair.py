from pathlib import Path
import hashlib,struct,tempfile,unittest
from tools import repair_bluetooth as r

class Repair(unittest.TestCase):
    def fixture(self,root):
        sd=root/'sd';config=b'[emummc]\nenabled=1\n'
        history=bytearray(64);history[:8]=b'BTASYN1\0';struct.pack_into('<II',history,8,1,0)
        history[16:48]=hashlib.sha256(config).digest();struct.pack_into('<I',history,48,0x220000)
        data={'emuMMC/emummc.ini':config,'switch/bt-autosync-v1/history.bin':bytes(history)+hashlib.sha256(history).digest(),
            'atmosphere/bluetooth_devices.db':bytes(8),'bootloader/payloads/BT-Sync-Una-Vez.bin':b'original payload',
            'switch/bt-autosync-v1/hekate-continue.bin':b'original continuation',
            'bootloader/hekate_ipl.ini':b'original menu','bootloader/update.bin':b'original loader',
            'atmosphere/package3':b'original package3','atmosphere/config/system_settings.ini':b'original settings',
            'switch/prod.keys':b'bis_key_02 = '+b'12'*32+b'\nsave_mac_key = '+b'34'*16+b'\n'}
        for name,blob in data.items():p=sd/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob)
        return sd,data

    def test_additional_payload_preserves_every_existing_file_and_backup_omits_keys(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,data=self.fixture(root);before,counts=r.inspect_existing(sd)
            self.assertEqual(counts,{'database_records':0,'history_records':0})
            plan={'sd':sd.resolve(),'before':before,'counts':counts,'payload':b'new compiled payload'}
            dest=r.apply_repair(plan,root/'backup','BT-Sync-Compat.bin')
            self.assertEqual(dest.read_bytes(),plan['payload'])
            for name,blob in data.items():self.assertEqual((sd/name).read_bytes(),blob)
            self.assertFalse((root/'backup/switch/prod.keys').exists())
            with self.assertRaises(ValueError):r.apply_repair(plan,root/'second-backup','BT-Sync-Compat.bin')
            for name,blob in data.items():self.assertEqual((sd/name).read_bytes(),blob)

    def test_pending_transaction_or_changed_profile_is_never_reset(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,data=self.fixture(root)
            txn=sd/'switch/bt-autosync-v1/transaction.bin';txn.write_bytes(b'pending')
            with self.assertRaisesRegex(ValueError,'pendiente'):r.inspect_existing(sd)
            self.assertEqual(txn.read_bytes(),b'pending');txn.unlink()
            (sd/'emuMMC/emummc.ini').write_bytes(b'changed profile')
            with self.assertRaisesRegex(ValueError,'perfil emuMMC'):r.inspect_existing(sd)
            self.assertEqual((sd/'switch/bt-autosync-v1/history.bin').read_bytes(),data['switch/bt-autosync-v1/history.bin'])

    def test_default_keeps_one_visible_payload_and_archives_both_previous_versions(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,data=self.fixture(root)
            compat=sd/'bootloader/payloads/BT-Sync-Compat.bin';compat.write_bytes(b'previous compatibility build')
            before,counts=r.inspect_existing(sd);plan={'sd':sd.resolve(),'before':before,'counts':counts,'payload':b'new canonical build'}
            dest=r.apply_repair(plan,root/'backup')
            self.assertEqual(dest.name,'BT-Sync-Una-Vez.bin');self.assertEqual(dest.read_bytes(),plan['payload'])
            self.assertFalse(compat.exists())
            self.assertEqual((root/'backup/bootloader/payloads/BT-Sync-Compat.bin').read_bytes(),b'previous compatibility build')
            self.assertEqual((root/'backup/bootloader/payloads/BT-Sync-Una-Vez.bin').read_bytes(),b'original payload')
            for name,blob in data.items():
                if name!='bootloader/payloads/BT-Sync-Una-Vez.bin':self.assertEqual((sd/name).read_bytes(),blob)

    def test_stale_target_and_bad_name_rejected_before_writing(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);sd,_=self.fixture(root);before,counts=r.inspect_existing(sd)
            plan={'sd':sd.resolve(),'before':before,'counts':counts,'payload':b'new'}
            with self.assertRaises(ValueError):r.apply_repair(plan,root/'backup','../escape.bin')
            (sd/'bootloader/hekate_ipl.ini').write_bytes(b'changed')
            with self.assertRaises(ValueError):r.apply_repair(plan,root/'backup')
            self.assertFalse((root/'backup').exists())
            self.assertFalse((sd/'bootloader/payloads/BT-Sync-Compat.bin').exists())

if __name__=='__main__':unittest.main()
