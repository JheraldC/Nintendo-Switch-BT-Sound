"""Real deployment planning against a synthetic prepared target."""
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import deploy_portable as d
class Plan(unittest.TestCase):
    def test_plan_and_rejections(self):
        sd=(d.OUT/'synthetic-target').resolve()
        plan,files=d.plan_for(sd,check_partition=False)
        self.assertEqual(len(files),5);self.assertEqual(plan['history_initial_count'],0)
        self.assertEqual(plan['existing_hack_records'],0)
        self.assertEqual(len(files['switch/bt-autosync-v1/history.bin']),96)
        for name in ['bootloader/hekate_ipl.ini','emuMMC/emummc.ini','atmosphere/config/system_settings.ini','switch/prod.keys','atmosphere/bluetooth_devices.db']:
            p=sd/name;before=p.read_bytes()
            try:
                p.write_bytes(b'CORRUPT')
                with self.assertRaises((ValueError,UnicodeError)):d.plan_for(sd,check_partition=False)
            finally:p.write_bytes(before)
        target=sd/'bootloader/payloads/bt-autosync-v1.bin';target.parent.mkdir(exist_ok=True)
        try:
            target.write_bytes(b'existing installation')
            with self.assertRaises(ValueError):d.plan_for(sd,check_partition=False)
        finally:target.unlink()
        self.assertEqual(plan,d.plan_for(sd,check_partition=False)[0])
if __name__=='__main__':unittest.main(verbosity=2)
