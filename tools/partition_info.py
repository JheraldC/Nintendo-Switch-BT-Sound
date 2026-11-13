"""Límites de la partición FAT32 montada, sin abrir ni escribir el dispositivo."""
from pathlib import Path
import json,os,plistlib,re,subprocess,sys

def bounds(offset,size,block_size=512):
    if not all(isinstance(v,int) and not isinstance(v,bool) for v in (offset,size,block_size)):
        raise ValueError('Geometría de partición no válida')
    if block_size!=512 or offset%512 or size%512 or not 0<offset//512<(offset+size)//512<=0xffffffff:
        raise ValueError('Se requiere una partición FAT32 con sectores de 512 bytes y límites válidos')
    return offset//512,(offset+size)//512

def partition_info(sd,system=None,run=subprocess.check_output,sysfs=Path('/sys/class/block')):
    system=system or ('windows' if os.name=='nt' else sys.platform)
    if system=='windows':
        if not re.fullmatch(r'[A-Za-z]:[/\\]?',str(sd)):raise ValueError('Usa la raíz de una unidad, por ejemplo E:/')
        letter=str(sd)[0]
        vol=json.loads(run(['powershell.exe','-NoProfile','-Command',f'Get-Volume -DriveLetter {letter} | Select-Object FileSystem | ConvertTo-Json']))
        if vol.get('FileSystem')!='FAT32':raise ValueError('La SD debe ser FAT32')
        p=json.loads(run(['powershell.exe','-NoProfile','-Command',f'Get-Partition -DriveLetter {letter} | Select-Object Offset,Size | ConvertTo-Json']))
        return bounds(p['Offset'],p['Size'])
    sd=Path(sd).resolve()
    if system=='darwin':
        p=plistlib.loads(run(['diskutil','info','-plist',str(sd)]))
        if p.get('FilesystemType')!='msdos' or 'FAT32' not in p.get('FilesystemName','').upper():raise ValueError('La SD debe ser MS-DOS FAT32')
        if not p.get('MountPoint') or Path(p['MountPoint']).resolve()!=sd or p.get('WholeDisk',False):raise ValueError('Selecciona la raíz del volumen montado de la SD')
        return bounds(p['PartitionMapPartitionOffset'],p['TotalSize'],p['DeviceBlockSize'])
    if system=='linux':
        data=json.loads(run(['findmnt','--json','--target',str(sd),'--output','SOURCE,FSTYPE,TARGET']))
        volumes=data.get('filesystems',[])
        if len(volumes)!=1:raise ValueError('No pude identificar una única partición montada')
        v=volumes[0]
        if v.get('fstype')!='vfat' or Path(v['target']).resolve()!=sd:raise ValueError('Selecciona la raíz de una partición FAT32 montada')
        device=Path(v['source']).resolve()
        if device.parent!=Path('/dev').resolve():raise ValueError('Se requiere una partición física de la SD')
        info=run(['lsblk','--noheadings','--nodeps','--output','FSTYPE,FSVER',str(device)]).decode().split()
        if info!=['vfat','FAT32']:raise ValueError('No pude confirmar FAT32; revisa la SD con lsblk -f')
        node=Path(sysfs)/device.name
        start=int((node/'start').read_text());size=int((node/'size').read_text())
        return bounds(start*512,size*512)
    raise ValueError('Sistema no admitido: '+system)
