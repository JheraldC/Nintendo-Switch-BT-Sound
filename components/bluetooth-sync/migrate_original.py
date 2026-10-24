"""Guarded conversion of the known legacy autosync installation; pairings preserved."""
from pathlib import Path
import argparse,hashlib,json,os,struct
import prepare as p
from deploy_portable import verify_keys,write_new,protected
from install_formats import parse_external

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'output'
BACKUP=ROOT.parent/'backup-original-before-manual'
LEGACY='bootloader/payloads/bt-autosync-v1.bin'
INNER='bootloader/ini/bt-autosync-v1.ini'
OLD_SHA='06e8cb5153b8b9064435b317b71da70f88903950bcc29c04bc300c0a75cb262a'
INNER_SHA='e1c2810b436c1c38cf3d04521d65c1b5c164e6872f7163ef0a7283748f45954a'
MAIN='bootloader/hekate_ipl.ini'
DB='atmosphere/bluetooth_devices.db'
STATE='switch/bt-autosync-v1/history.bin'
GUIDE='switch/bt-autosync-v1/LEEME.txt'
CONTINUE='switch/bt-autosync-v1/hekate-continue.bin'
def sha(data):return hashlib.sha256(data).hexdigest()

def remove_hook(data):
    block=p.section_bytes(data,'Sistema Hack')
    nl=b'\r\n' if b'\r\n' in block else b'\n'
    hook=b'payload=bootloader/payloads/bt-autosync-v1.bin'+nl
    p.require(block.count(hook)==1,'Expected exactly one legacy Hack hook')
    after=data.replace(block,block.replace(hook,b'',1),1)
    p.require(p.section_bytes(after,'Sistema Original')==p.section_bytes(data,'Sistema Original'),'Original entry changed')
    validated,inner=p.boot_configs(after,'Sistema Original','Sistema Hack')
    p.require(validated==after and inner==b'','Unexpected boot conversion')
    return after

def validate_history(data,cfg):
    p.require(len(data)>=96 and data[:8]==b'BTASYN1\0','Invalid history header')
    version,count=struct.unpack_from('<II',data,8)
    p.require(version==1 and count<=24 and len(data)==96+40*count,'Invalid history geometry')
    p.require(data[16:48]==cfg and struct.unpack_from('<I',data,48)[0]==0x220000 and not any(data[52:64]),'Wrong history profile/config')
    p.require(hashlib.sha256(data[:-32]).digest()==data[-32:],'Invalid history checksum')
    seen=set()
    for i in range(count):
        e=data[64+40*i:104+40*i];address=e[:6]
        p.require(any(address) and address not in seen and e[6]<=1 and e[7]==0 and any(e[8:40]),'Invalid history record')
        seen.add(address)
    return count

def prepare(sd):
    p.require(not BACKUP.exists(),'Use a fresh backup directory; do not overwrite an earlier preparation')
    p.require(sha((sd/LEGACY).read_bytes())==OLD_SHA and sha((sd/INNER).read_bytes())==INNER_SHA,'Not the verified legacy installation')
    p.require(not (sd/p.PAYLOAD_PATH).exists(),'Manual payload already exists; review instead of overwrite')
    for name in ('atmosphere/bluetooth_devices.bt-next','atmosphere/bluetooth_devices.bt-prev','switch/bt-autosync-v1/history.next','switch/bt-autosync-v1/history.prev','switch/bt-autosync-v1/transaction.bin'):
        p.require(not (sd/name).exists(),'Unfinished transaction: recover before migration')
    before=(sd/MAIN).read_bytes();after=remove_hook(before)
    config=(sd/'emuMMC/emummc.ini').read_bytes();settings=(sd/'atmosphere/config/system_settings.ini').read_bytes()
    p.require(p.parse(config)['emummc'].get('enabled')=='1','emuMMC must stay enabled')
    p.require(p.parse(settings)['atmosphere'].get('enable_external_bluetooth_db')=='u8!0x1','External Bluetooth DB required')
    cfg=hashlib.sha256(config).digest();state_count=validate_history((sd/STATE).read_bytes(),cfg)
    count=len(parse_external((sd/DB).read_bytes()));verify_keys((sd/'switch/prod.keys').read_bytes())
    hekate=(sd/'bootloader/update.bin').read_bytes()
    p.require(sha(hekate)==p.HEKATE_SHA and (sd/CONTINUE).read_bytes()==hekate,'Pinned Hekate continuation required')
    first,end=p.partition(sd)
    OUT.mkdir(exist_ok=True)
    for name,data in {'hekate_ipl.before.ini':before,'hekate_ipl.after.ini':after,'hekate-continue.bin':hekate}.items():(OUT/name).write_bytes(data)
    header='#ifndef BTA_BINDING_H\n#define BTA_BINDING_H\n'
    for name,data in [('bta_config_hash',config),('bta_settings_hash',settings),('bta_main_ini_hash',after),('bta_hekate_hash',hekate)]:
        header+='static const unsigned char '+name+'[32]={'+','.join('0x%02x'%v for v in hashlib.sha256(data).digest())+'};\n'
    header+=f'#define BTA_HEKATE_BYTES {len(hekate)}u\n#define BTA_SD_FIRST {first}u\n#define BTA_SD_END {end}u\n#endif\n'
    (ROOT/'include/auto_binding.h').write_text(header)
    snapshot=protected(sd)
    for name in (MAIN,STATE,GUIDE,CONTINUE):snapshot[name]=sha((sd/name).read_bytes())
    backup_names=(MAIN,DB,STATE,GUIDE,CONTINUE,LEGACY,INNER,'emuMMC/emummc.ini','atmosphere/config/system_settings.ini')
    BACKUP.mkdir(parents=True)
    backup_hashes={}
    for name in backup_names:
        data=(sd/name).read_bytes();write_new(BACKUP/name,data);backup_hashes[name]=sha(data)
    write_new(BACKUP/'BACKUP-SHA256.json',json.dumps(backup_hashes,indent=2).encode())
    plan={'sd':str(sd),'sd_first':first,'sd_end':end,'backup':str(BACKUP),'backup_sha256':backup_hashes,'protected':snapshot,
          'before_sha256':sha(before),'after_sha256':sha(after),'binding_sha256':sha((ROOT/'include/auto_binding.h').read_bytes()),
          'script_sha256':sha(Path(__file__).read_bytes()),'existing_pairing_records':count,'existing_history_records':state_count,
          'history_preserved':True,'pairings_preserved':True,'removed_hook_only':True,'summary_waits_for_volume_minus':True,
          'archive_from_sd':[LEGACY,INNER],'new_payload':p.PAYLOAD_PATH}
    (OUT/'migration-plan.json').write_text(json.dumps(plan,indent=2))
    print('PREPARED: original SD; verified backup; pairings/history preserved; only legacy Hack hook will be removed. Build, test and audit before apply.')

def apply(sd):
    plan=json.loads((OUT/'migration-plan.json').read_text())
    p.require(str(sd)==plan['sd'] and p.partition(sd)==(plan['sd_first'],plan['sd_end']),'Wrong SD/partition')
    p.require(sha(Path(__file__).read_bytes())==plan['script_sha256'],'Migration code changed; review again')
    p.require(all(sha((sd/name).read_bytes())==h for name,h in plan['protected'].items()),'SD changed since preparation')
    p.require(all(sha((BACKUP/name).read_bytes())==h for name,h in plan['backup_sha256'].items()),'Backup verification failed')
    tests=json.loads((OUT/'test-results.json').read_text());audit=json.loads((OUT/'auto-audit.json').read_text());build=json.loads((OUT/'build-evidence.json').read_text())
    p.require(tests['passed'] and tests['asan_ubsan'] and audit['passed'],'Tests/audit required')
    p.require(all(sha((ROOT/name).read_bytes())==h for name,h in tests['tested_sources'].items()),'Stale tests')
    p.require(all(sha((ROOT/name).read_bytes())==h for name,h in build['compiled_sources'].items()),'Stale build')
    p.require(sha((ROOT/'include/auto_binding.h').read_bytes())==plan['binding_sha256']==build['binding_sha256'],'Binding/build mismatch')
    binary=(OUT/'bt-autosync-v1.bin').read_bytes();p.require(sha(binary)==build['sha256']==audit['sha256'],'Binary/audit mismatch')
    before=(OUT/'hekate_ipl.before.ini').read_bytes();after=(OUT/'hekate_ipl.after.ini').read_bytes()
    p.require(sha(before)==plan['before_sha256'] and sha(after)==plan['after_sha256'] and remove_hook(before)==after,'Boot conversion changed')
    write_new(sd/p.PAYLOAD_PATH,binary)
    main_stage=sd/'bootloader/.bt-manual-convert.new';guide_stage=sd/'switch/bt-autosync-v1/.manual-guide.new'
    write_new(main_stage,after);write_new(guide_stage,(ROOT/'LEEME.txt').read_bytes())
    p.require(all(sha((sd/name).read_bytes())==h for name,h in plan['protected'].items()),'SD changed before activation')
    os.replace(guide_stage,sd/GUIDE);os.replace(main_stage,sd/MAIN)
    p.require((sd/MAIN).read_bytes()==after and (sd/p.PAYLOAD_PATH).read_bytes()==binary,'Activation readback failed')
    for name in plan['archive_from_sd']:
        target=(sd/name).resolve();p.require(target.is_relative_to(sd) and sha(target.read_bytes())==plan['backup_sha256'][name],'Legacy archive mismatch')
        target.unlink()
    changed={MAIN,GUIDE,*plan['archive_from_sd']}
    p.require(all(sha((sd/name).read_bytes())==h for name,h in plan['protected'].items() if name not in changed),'Protected data changed')
    result={**plan,'installed':True,'payload_sha256':sha(binary),'payload_bytes':len(binary),'protected_files_unchanged':len(plan['protected'])-len(changed),'pairing_sync_executed_on_pc':False,'hardware_validation_pending':True}
    (OUT/'migration-result.json').write_text(json.dumps(result,indent=2))
    print('CONVERTED: payload installed; automatic Hack hook removed; Original unchanged; pairing DB/history unchanged; summary waits for VOL-.')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--sd',required=True);ap.add_argument('--apply',action='store_true');args=ap.parse_args()
    sd=Path(args.sd).resolve()
    apply(sd) if args.apply else prepare(sd)
