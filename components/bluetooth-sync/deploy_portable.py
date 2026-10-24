"""Install a standalone manual payload. Menus untouched; verified backup mandatory."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import struct
from datetime import datetime,timezone
from prepare import ROOT,OUT,HEKATE_SHA,PAYLOAD_PATH,partition,require,section_bytes,boot_configs
from install_formats import parse_external

def sha(data):return hashlib.sha256(data).hexdigest()
def write_new(p,data):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
    require(p.read_bytes()==data,'File readback failed')
def empty_history(cfg):
    state=bytearray(64);state[:8]=b'BTASYN1\0';struct.pack_into('<II',state,8,1,0);state[16:48]=cfg;struct.pack_into('<I',state,48,0x220000)
    return bytes(state)+hashlib.sha256(state).digest()
def verify_keys(data):
    import re
    require(len(data)<=256*1024,'Key file too large')
    text=data.decode('utf-8-sig')
    for name,size in [('bis_key_02',32),('save_mac_key',16)]:
        matches=re.findall(r'^\s*'+name+r'\s*=\s*([0-9a-fA-F]+)\s*$',text,re.M)
        require(len(matches)==1 and len(matches[0])==size*2,'Missing/invalid required local key')
        value=bytes.fromhex(matches[0]);require(any(value),'Empty required key')
        if name=='bis_key_02':require(any(value[:16]) and any(value[16:]),'Empty BIS key half')
def protected(sd):
    paths=set()
    for folder in ('bootloader','atmosphere/config','atmosphere/contents','atmosphere/exefs_patches','config/JheraldBootSound'):
        paths.update(p for p in (sd/folder).rglob('*') if p.is_file() and p!=sd/'bootloader/hekate_ipl.ini')
    for name in ('atmosphere/package3','atmosphere/reboot_payload.bin','atmosphere/bluetooth_devices.db','emuMMC/emummc.ini'):
        p=sd/name
        if p.is_file():paths.add(p)
    return {p.relative_to(sd).as_posix():sha(p.read_bytes()) for p in sorted(paths)}

def backup_target(sd,backup,plan):
    """Keep target pairings private, outside SD, and verify before activation."""
    sd=Path(sd).resolve();backup=Path(backup).resolve()
    require(not backup.exists() and not backup.is_relative_to(sd),'Backup must be a NEW directory outside SD')
    names=('bootloader/hekate_ipl.ini','emuMMC/emummc.ini',
           'atmosphere/config/system_settings.ini','atmosphere/bluetooth_devices.db')
    copies={name:(sd/name).read_bytes() for name in names}
    for name,data in copies.items():
        expected=plan['before_sha256'] if name==names[0] else plan['protected'].get(name)
        require(expected is not None and sha(data)==expected,'Target changed before pairing backup')
    copies['hekate_ipl.before.ini']=copies[names[0]]
    copies['installation-plan.json']=json.dumps(plan,indent=2).encode()
    backup.mkdir(parents=True)
    for name,data in copies.items():write_new(backup/name,data)
    manifest={name:sha(data) for name,data in copies.items()}
    write_new(backup/'BACKUP-SHA256.json',json.dumps(manifest,indent=2).encode())
    require(all(sha((backup/name).read_bytes())==digest for name,digest in manifest.items()),'Backup verification failed')
    return manifest

def plan_for(sd,check_partition=True):
    prep=json.loads((OUT/'preparation.json').read_text());require(Path(prep['sd']).resolve()==sd,'Prepared for a different SD root')
    require(prep.get('mode')=='standalone-payload' and prep.get('launch_payload')==PAYLOAD_PATH,'Standalone payload preparation required')
    if check_partition:require(partition(sd)==(prep['sd_first'],prep['sd_end']),'Partition changed')
    require(sha((ROOT/'include/auto_binding.h').read_bytes())==prep['binding_sha256'],'Bindings changed; rebuild')
    for name,h in prep['artifacts'].items():require(sha((OUT/name).read_bytes())==h,'Prepared artifact changed')
    require((sd/'bootloader/hekate_ipl.ini').read_bytes()==(OUT/'hekate_ipl.before.ini').read_bytes(),'Boot config changed')
    expected_after,expected_inner=boot_configs((OUT/'hekate_ipl.before.ini').read_bytes(),prep['original_entry'],prep['hack_entry'])
    require((OUT/'hekate_ipl.after.ini').read_bytes()==expected_after==(OUT/'hekate_ipl.before.ini').read_bytes() and expected_inner==b'','Boot menu must remain unchanged')
    for name,h in [('emuMMC/emummc.ini',prep['config_sha256']),('atmosphere/config/system_settings.ini',prep['settings_sha256']),('bootloader/update.bin',HEKATE_SHA)]:
        require(sha((sd/name).read_bytes())==h,'Target config/loader changed')
    for name,h in prep.get('boot_dependencies',{}).items():require(sha((sd/name).read_bytes())==h,'Existing boot component changed')
    import re
    for p in (sd/'bootloader/ini').glob('*.ini'):require(not re.search(rb'(?m)^\s*id\s*=\s*BTREAL\s*$',p.read_bytes()),'BTREAL ID collision')
    audit=json.loads((OUT/'auto-audit.json').read_text());build=json.loads((OUT/'build-evidence.json').read_text());tests=json.loads((OUT/'test-results.json').read_text())
    require(audit['passed'] and tests['passed'] and tests['asan_ubsan'],'Audit/tests required')
    binary=(OUT/'bt-autosync-v1.bin').read_bytes();require(sha(binary)==audit['sha256']==build['sha256'],'Binary evidence mismatch')
    require(sha((ROOT/'include/auto_binding.h').read_bytes())==build['binding_sha256'],'Build predates bindings')
    for name,h in build['compiled_sources'].items():require(sha((ROOT/name).read_bytes())==h,'Stale build')
    for name,h in tests['tested_sources'].items():require(sha((ROOT/name).read_bytes())==h,'Stale tests')
    verify_keys((sd/'switch/prod.keys').read_bytes())
    # Existing pairing DB remains on its own SD. Only count is reported.
    count=len(parse_external((sd/'atmosphere/bluetooth_devices.db').read_bytes()))
    additions={PAYLOAD_PATH:binary,
               'switch/bt-autosync-v1/hekate-continue.bin':(OUT/'hekate-continue.bin').read_bytes(),
               'switch/bt-autosync-v1/history.bin':empty_history(bytes.fromhex(prep['config_sha256'])),
               'switch/bt-autosync-v1/LEEME.txt':(ROOT/'LEEME.txt').read_bytes()}
    require(all(not (sd/name).exists() for name in additions),'Existing installation: do not overwrite')
    for name in ('atmosphere/bluetooth_devices.bt-next','atmosphere/bluetooth_devices.bt-prev',
                 'switch/bt-autosync-v1/history.next','switch/bt-autosync-v1/history.prev','switch/bt-autosync-v1/transaction.bin'):
        require(not (sd/name).exists(),'Existing transaction state: review before installing')
    require(not (sd/'bootloader/.bt-autosync-v1.ipl.new').exists(),'Unexpected staging file')
    plan={'sd':str(sd),'additions':{name:sha(data) for name,data in additions.items()},'protected':protected(sd),
          'before_sha256':prep['artifacts']['hekate_ipl.before.ini'],'after_sha256':prep['artifacts']['hekate_ipl.after.ini'],
          'original_entry':prep['original_entry'],'hack_entry':prep['hack_entry'],'mode':'standalone-payload','launch_payload':PAYLOAD_PATH,'boot_menu_modified':False,
          'normal_hack_boot_runs_sync':False,'existing_hack_records':count,'history_initial_count':0,
          'continuation_mode':prep['continuation_mode'],'boot_dependencies':prep['boot_dependencies'],
          'first_successful_read_imports_all_supported_original_controllers':True,'deletions':[]}
    return plan,additions
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sd',required=True);p.add_argument('--apply',action='store_true');p.add_argument('--backup-dir');a=p.parse_args();sd=Path(a.sd).resolve()
    plan,additions=plan_for(sd)
    if not a.apply:
        (OUT/'deployment-plan.json').write_text(json.dumps(plan,indent=2));print('DRY RUN: 4 additions; standalone BT-Sync-Una-Vez.bin payload; NO menu edits; DB preserved; verified external backup required.');return
    require(a.backup_dir is not None,'Use a new --backup-dir on the PC')
    require(plan==json.loads((OUT/'deployment-plan.json').read_text()),'Plan/SD changed')
    backup=Path(a.backup_dir).resolve();backup_manifest=backup_target(sd,backup,plan)
    before=(OUT/'hekate_ipl.before.ini').read_bytes()
    after=(OUT/'hekate_ipl.after.ini').read_bytes();require(after==before,'Boot menu modification forbidden')
    for name,data in additions.items():write_new(sd/name,data)
    require((sd/'bootloader/hekate_ipl.ini').read_bytes()==before,'Boot config changed during installation')
    require(all(sha((sd/name).read_bytes())==h for name,h in plan['protected'].items()),'Protected file changed')
    actual=(sd/'bootloader/hekate_ipl.ini').read_bytes();require(actual==after and all(section_bytes(actual,plan[entry])==section_bytes(before,plan[entry]) for entry in ('original_entry','hack_entry')),'Activation readback failed')
    require(all((sd/name).read_bytes()==data for name,data in additions.items()),'Installed artifact changed')
    require(all(sha((sd/name).read_bytes())==h for name,h in plan['protected'].items()),'Protected file changed')
    result={**plan,'applied':True,'backup':str(backup),'backup_sha256':backup_manifest,'installed_utc':datetime.now(timezone.utc).isoformat(),'hardware_tested_on_target':False}
    write_new(backup/'installation.json',json.dumps(result,indent=2).encode());(OUT/'deployment-result.json').write_text(json.dumps(result,indent=2))
    print('INSTALLED: standalone BT-Sync-Una-Vez.bin; NO boot menu writes; target pairings backed up and preserved. Launch from Hekate Payloads once, then use normal Hack. Physical tests required.')
if __name__=='__main__':main()
