"""Legacy menu rollback. Standalone edition needs none; pairings are not restored."""
from pathlib import Path
import argparse,json,os
from prepare import require
from deploy_portable import sha,write_new
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sd',required=True);p.add_argument('--backup-dir',required=True);a=p.parse_args()
    sd=Path(a.sd).resolve();backup=Path(a.backup_dir).resolve();report=json.loads((backup/'installation.json').read_text());before=(backup/'hekate_ipl.before.ini').read_bytes()
    require(Path(report['sd']).resolve()==sd and sha(before)==report['before_sha256'],'Wrong backup/SD')
    if report.get('mode')=='standalone-payload':
        print('This standalone payload never edited boot menus. No menu restoration needed; current pairings preserved.');return
    main=sd/'bootloader/hekate_ipl.ini';require(sha(main.read_bytes())==report['after_sha256'],'Boot configuration changed; manual review required')
    stage=sd/'bootloader/.bt-autosync-v1.rollback.new';write_new(stage,before);os.replace(stage,main);require(main.read_bytes()==before,'Rollback readback failed')
    print('Manual menu entry removed. Current Hack Bluetooth DB/history preserved. Normal Hack stays direct; no NAND access.')
if __name__=='__main__':main()
