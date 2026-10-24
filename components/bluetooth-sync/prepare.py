"""Bind public artifacts to ONE target SD. Reads no pairing records or keys."""
from pathlib import Path
import argparse
import configparser
import hashlib
import json
import os
import re
import subprocess

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'output'
HEKATE_SHA='95f6e933e32e028f3ae90f0b0857ff47800e834bdace38a0ab019d8ed35a8c24'
MANUAL_ENTRY='Sincronizar Bluetooth una vez'
PAYLOAD_PATH='bootloader/payloads/BT-Sync-Una-Vez.bin'

def require(ok,message):
    if not ok:raise ValueError(message)

def parse(data):
    c=configparser.ConfigParser(interpolation=None,strict=True)
    c.read_string(data.decode('utf-8-sig'));return c

def section_bytes(data,name):
    m=re.search(rb'(?m)^\['+re.escape(name.encode())+rb'\]\r?\n',data)
    require(m is not None,'Boot entry not found: '+name)
    tail=re.search(rb'(?m)^\[',data[m.end():]);end=m.end()+tail.start() if tail else len(data)
    return data[m.start():end]

def boot_configs(ipl,original,hack):
    require(original!=hack and all(c not in original+hack for c in '\r\n[]'),'Invalid entry names')
    c=parse(ipl);src=c[original];dst=c[hack]
    require(c.has_section('config') and c['config'].get('autoboot_list','0')=='0','External-list autoboot requires separate review')
    autoboot=c['config'].get('autoboot','0')
    require(autoboot.isdecimal() and 0<=int(autoboot)<len(c.sections()),'Existing main autoboot index is invalid; review before appending an entry')
    require(MANUAL_ENTRY not in c,'Manual synchronization entry already exists')
    require(ipl.endswith(b'\n'),'Save the boot INI with a final newline before preparation')
    require(src.get('stock')=='1' and src.get('emummc_force_disable')=='1' and 'payload' not in src,'Original must be a direct stock sysMMC entry')
    require('id' not in dst,'Hack already has an ID; review manually')
    if 'payload' in dst:
        require(dst.get('payload')=='bootloader/payloads/fusee.bin' and set(dst)<= {'payload','icon'},'Only the existing plain Fusee boot route is supported')
    else:
        require(dst.get('emummcforce')=='1' and dst.get('emummc_force_disable','0')=='0' and dst.get('stock','0')=='0','Hack must force emuMMC')
        require(dst.get('pkg3',dst.get('fss0'))=='atmosphere/package3','Unsupported package3 location')
    require(not re.search(rb'(?m)^\s*id\s*=\s*BTREAL\s*$',ipl),'BTREAL ID already exists')
    block=section_bytes(ipl,hack);header=re.match(rb'[^\n]*\n',block);require(header is not None,'Invalid entry header')
    nl=b'\r\n' if header[0].endswith(b'\r\n') else b'\n'
    after=ipl
    require(after[:len(ipl)]==ipl,'Unexpected boot edit')
    require(section_bytes(after,original)==section_bytes(ipl,original),'Original changed')
    require(section_bytes(after,hack)==block,'Normal Hack entry changed')
    inner=b''  # Standalone payload returns to Hekate; no auxiliary boot entry.
    return after,inner

def partition(sd):
    require(os.name=='nt','This first installer supports Windows drive letters only')
    drive=sd.drive
    require(re.fullmatch('[A-Za-z]:',drive) is not None and sd==Path(drive+'/'),'Use a drive root such as E:/')
    letter=drive[0]
    cmd='Get-Volume -DriveLetter '+letter+' | Select-Object FileSystem,FileSystemLabel | ConvertTo-Json'
    volume=json.loads(subprocess.check_output(['powershell.exe','-NoProfile','-Command',cmd],text=True))
    require(volume['FileSystem']=='FAT32','Target must be FAT32')
    cmd='Get-Partition -DriveLetter '+letter+' | Select-Object Offset,Size | ConvertTo-Json'
    part=json.loads(subprocess.check_output(['powershell.exe','-NoProfile','-Command',cmd],text=True))
    offset,size=part['Offset'],part['Size']
    require(offset%512==size%512==0 and 0<offset//512<(offset+size)//512<=0xffffffff,'Invalid partition bounds')
    return offset//512,(offset+size)//512

def prepare(sd,original,hack,first,end,declared_profile):
    require(declared_profile=='23.0.1','Only the analyzed 23.0.1 settings profile is supported')
    config=(sd/'emuMMC/emummc.ini').read_bytes();settings=(sd/'atmosphere/config/system_settings.ini').read_bytes();ipl=(sd/'bootloader/hekate_ipl.ini').read_bytes()
    require(parse(config)['emummc'].get('enabled')=='1','Enable emuMMC first')
    require(parse(settings)['atmosphere'].get('enable_external_bluetooth_db')=='u8!0x1','Enable external Bluetooth DB and boot Hack once first')
    require((sd/'atmosphere/bluetooth_devices.db').is_file(),'Boot Hack with external Bluetooth DB enabled first')
    require((sd/'switch/prod.keys').is_file(),'This console must have its own local prod.keys; never share keys')
    after,inner=boot_configs(ipl,original,hack)
    for p in (sd/'bootloader/ini').glob('*.ini'):
        require(not re.search(rb'(?m)^\s*id\s*=\s*BTREAL\s*$',p.read_bytes()),'BTREAL ID collision')
    hekate=(sd/'bootloader/update.bin').read_bytes()
    require(hashlib.sha256(hekate).hexdigest()==HEKATE_SHA,'Exact Hekate 6.5.4 release binary required')
    require(0<first<end<=0xffffffff,'Invalid partition bounds')
    dependencies={'atmosphere/package3':hashlib.sha256((sd/'atmosphere/package3').read_bytes()).hexdigest()} if (sd/'atmosphere/package3').is_file() else {}
    fusee_boot=parse(ipl)[hack].get('payload')=='bootloader/payloads/fusee.bin'
    if fusee_boot:
        fusee=(sd/'bootloader/payloads/fusee.bin').read_bytes()
        require(0<len(fusee)<=126296,'Invalid Fusee payload size')
        dependencies['bootloader/payloads/fusee.bin']=hashlib.sha256(fusee).hexdigest()
    OUT.mkdir(exist_ok=True)
    artifacts={'hekate_ipl.before.ini':ipl,'hekate_ipl.after.ini':after,'hekate-continue.bin':hekate}
    for name,data in artifacts.items():(OUT/name).write_bytes(data)
    header='#ifndef BTA_BINDING_H\n#define BTA_BINDING_H\n'
    bindings=[('bta_config_hash',config),('bta_settings_hash',settings),('bta_main_ini_hash',after),('bta_hekate_hash',hekate)]
    for name,data in bindings:
        header+='static const unsigned char '+name+'[32]={'+','.join('0x%02x'%v for v in hashlib.sha256(data).digest())+'};\n'
    header+=f'#define BTA_HEKATE_BYTES {len(hekate)}u\n#define BTA_SD_FIRST {first}u\n#define BTA_SD_END {end}u\n#endif\n'
    (ROOT/'include/auto_binding.h').write_text(header)
    info={'sd':str(sd),'original_entry':original,'hack_entry':hack,'declared_firmware_profile':declared_profile,
          'mode':'standalone-payload','launch_payload':PAYLOAD_PATH,
          'continuation_mode':'hekate-menu','boot_dependencies':dependencies,
          'sd_first':first,'sd_end':end,'config_sha256':hashlib.sha256(config).hexdigest(),
          'settings_sha256':hashlib.sha256(settings).hexdigest(),'binding_sha256':hashlib.sha256(header.encode()).hexdigest(),
          'artifacts':{name:hashlib.sha256(data).hexdigest() for name,data in artifacts.items()},'hardware_tested_on_this_target':False}
    # Header encoding must match write_text on Windows too (newline translation).
    info['binding_sha256']=hashlib.sha256((ROOT/'include/auto_binding.h').read_bytes()).hexdigest()
    (OUT/'preparation.json').write_text(json.dumps(info,indent=2))
    return info

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sd',required=True);p.add_argument('--original',required=True);p.add_argument('--hack',required=True);p.add_argument('--firmware-profile',required=True,choices=['23.0.1']);a=p.parse_args()
    sd=Path(a.sd).resolve();first,end=partition(sd)
    prepare(sd,a.original,a.hack,first,end,a.firmware_profile)
    print('Prepared standalone payload for this SD. Boot menus stay byte-identical; completion returns to Hekate. No SD files changed.')
if __name__=='__main__':main()
