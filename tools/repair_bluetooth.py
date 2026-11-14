"""Actualiza un único sincronizador; respalda su versión anterior y conserva el estado."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:sys.path.insert(0,str(ROOT))
BT=ROOT/'components/bluetooth-sync'
sys.path.insert(0,str(BT))
import prepare
from deploy_portable import verify_keys
from install_formats import parse_external
from tools.install_sd import safe,require
from tools.bluetooth_setup import verified_host_evidence
from tools.portable_runtime import ensure_arm_toolchain

PROTECTED=('bootloader/payloads/BT-Sync-Una-Vez.bin','switch/bt-autosync-v1/hekate-continue.bin',
 'switch/bt-autosync-v1/history.bin','switch/bt-autosync-v1/history.prev',
 'atmosphere/bluetooth_devices.db','atmosphere/bluetooth_devices.bt-prev',
 'bootloader/hekate_ipl.ini','bootloader/update.bin','atmosphere/package3',
 'emuMMC/emummc.ini','atmosphere/config/system_settings.ini')
def sha(data):return hashlib.sha256(data).hexdigest()

def inspect_existing(sd):
    sd=Path(sd).resolve()
    require((sd/PROTECTED[0]).is_file(),'No existe la instalación anterior; usa la instalación nueva')
    for name in ('switch/bt-autosync-v1/transaction.bin','switch/bt-autosync-v1/history.next','atmosphere/bluetooth_devices.bt-next'):
        require(not safe(sd,name).exists(),'Hay una transacción pendiente; conservarla y revisar antes de reparar')
    history=safe(sd,'switch/bt-autosync-v1/history.bin').read_bytes()
    config=safe(sd,'emuMMC/emummc.ini').read_bytes()
    require(len(history)>=96 and history[:8]==b'BTASYN1\0','Historial no reconocido')
    version,count=struct.unpack_from('<II',history,8)
    require(version==1 and count<=24 and len(history)==96+40*count,'Historial incompleto')
    require(sha(history[:-32])==history[-32:].hex(),'Historial corrupto; no se reinicia automáticamente')
    require(history[16:48]==hashlib.sha256(config).digest(),'Cambió el perfil emuMMC; requiere revisar el historial por separado')
    require(struct.unpack_from('<I',history,48)[0]==0x220000 and not any(history[52:64]),'Perfil de historial desconocido')
    seen=set()
    for index in range(count):
        entry=history[64+40*index:104+40*index]
        require(any(entry[:6]) and entry[:6] not in seen and entry[6] in (0,1) and entry[7]==0 and any(entry[8:]),'Entrada de historial no válida')
        seen.add(entry[:6])
    records=len(parse_external(safe(sd,'atmosphere/bluetooth_devices.db').read_bytes()))
    verify_keys(safe(sd,'switch/prod.keys').read_bytes())
    names=list(PROTECTED)
    for path in (sd/'bootloader/payloads').glob('BT-Sync-Compat*.bin'):
        if re.fullmatch(r'BT-Sync-Compat(?:-[A-Za-z0-9_-]+)?\.bin',path.name):names.append(path.relative_to(sd).as_posix())
    before={name:sha(safe(sd,name).read_bytes()) for name in names if safe(sd,name).is_file()}
    return before,{'database_records':records,'history_records':count}

def repair_plan(sd,original,hack,profile='23.0.1',controller_profile='nintendo'):
    sd=Path(sd).resolve();before,counts=inspect_existing(sd)
    first,end=prepare.partition(sd)
    info=prepare.prepare(sd,original,hack,first,end,profile,controller_profile,'existing')
    verified_host_evidence();ensure_arm_toolchain()
    for script in ('build_payload.py','audit_auto.py'):
        subprocess.run([sys.executable,script],cwd=BT,check=True)
    payload=(BT/'output/bt-autosync-v1.bin').read_bytes()
    audit=json.loads((BT/'output/auto-audit.json').read_text())
    require(audit['passed'] and audit['sha256']==sha(payload),'La auditoría del nuevo payload no coincide')
    for name,digest in before.items():require(sha(safe(sd,name).read_bytes())==digest,'La SD cambió durante la preparación')
    return {'sd':sd,'before':before,'counts':counts,'payload':payload,'profile':info}

def apply_repair(plan,backup,payload_name='BT-Sync-Una-Vez.bin'):
    sd=plan['sd'];backup=Path(backup).resolve()
    canonical=payload_name=='BT-Sync-Una-Vez.bin'
    require(canonical or re.fullmatch(r'BT-Sync-Compat(?:-[A-Za-z0-9_-]+)?\.bin',payload_name) is not None,'Nombre de payload no válido')
    require(not backup.exists() and not backup.is_relative_to(sd) and not backup.is_relative_to(ROOT),'Usa un respaldo NUEVO fuera de la SD y del repositorio')
    destination=safe(sd,'bootloader/payloads/'+payload_name)
    if not canonical:require(not destination.exists(),'El payload adicional ya existe; elige otro nombre para conservarlo')
    for name,digest in plan['before'].items():require(sha(safe(sd,name).read_bytes())==digest,'La SD cambió; revisa otro plan')
    backup.mkdir(parents=True)
    for name,digest in plan['before'].items():
        path=backup/name;path.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(safe(sd,name),path);require(sha(path.read_bytes())==digest,'El respaldo no coincide')
    (backup/'BEFORE-SHA256.json').write_text(json.dumps(plan['before'],indent=2),encoding='utf-8')
    relative='bootloader/payloads/'+payload_name
    retired=[name for name in plan['before'] if name.startswith('bootloader/payloads/BT-Sync-Compat') ] if canonical else []
    temporary=destination.with_name(destination.name+'.repair-new')
    require(not temporary.exists(),'Existe una preparación incompleta; revisar antes de continuar')
    created=False;replaced=False;staged=False;removed=[]
    try:
        if canonical:
            with temporary.open('xb') as target:staged=True;target.write(plan['payload']);target.flush();os.fsync(target.fileno())
            require(sha(temporary.read_bytes())==sha(plan['payload']),'Archivo temporal no verificado')
            require(sha(destination.read_bytes())==plan['before'][relative],'La versión anterior cambió')
            os.replace(temporary,destination);replaced=True
        else:
            with destination.open('xb') as target:
                created=True;target.write(plan['payload']);target.flush();os.fsync(target.fileno())
        require(sha(destination.read_bytes())==sha(plan['payload']),'La lectura del nuevo payload no coincide')
        for name,digest in plan['before'].items():
            if canonical and name==relative:continue
            require(sha(safe(sd,name).read_bytes())==digest,'Se modificó un archivo anterior')
        for name in retired:
            require(sha((backup/name).read_bytes())==plan['before'][name],'Respaldo de versión alternativa no verificado')
            safe(sd,name).unlink();removed.append(name)
    except Exception:
        if replaced:destination.write_bytes((backup/relative).read_bytes())
        for name in removed:safe(sd,name).write_bytes((backup/name).read_bytes())
        if created and destination.exists():destination.unlink()
        raise
    finally:
        if staged and temporary.exists():temporary.unlink()
    print('Sincronizador verificado:',destination)
    print('Versión anterior respaldada en PC. Retorno, base e historial conservados. Solo debes abrir este payload.')
    return destination

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sd',required=True);p.add_argument('--sysmmc',required=True);p.add_argument('--emummc',required=True)
    p.add_argument('--firmware-profile',default='23.0.1',choices=['23.0.1'])
    p.add_argument('--controller-profile',choices=prepare.CONTROLLER_PROFILES,default='nintendo')
    p.add_argument('--payload-name',default='BT-Sync-Una-Vez.bin',help='Nombre único habitual; otro nombre Compat es solo un modo avanzado');p.add_argument('--apply',action='store_true');p.add_argument('--backup-dir')
    a=p.parse_args();plan=repair_plan(a.sd,a.sysmmc,a.emummc,a.firmware_profile,a.controller_profile)
    print('Preparado:',a.payload_name,'Perfil:',a.controller_profile,'Registros conservados:',plan['counts'])
    if not a.apply:print('Solo revisión. No se escribió en la SD.');return
    require(a.backup_dir,'Falta --backup-dir');apply_repair(plan,a.backup_dir,a.payload_name)

if __name__=='__main__':main()
