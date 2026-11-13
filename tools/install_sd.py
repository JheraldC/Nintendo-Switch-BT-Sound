"""Instalación limitada a sonido o logo. Sin --apply solo muestra el plan."""
from pathlib import Path
import argparse, configparser, hashlib, json, os, re, sys, tempfile

ROOT=Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:sys.path.insert(0,str(ROOT))
from tools.audio_convert import validate_wav,convert_audio
MODULE='atmosphere/contents/42000000004A4253'
CONFIG='config/JheraldBootSound'
NSP_SHA='7fdc7c9f0d2b3ff053ce813fd541cfdf024065d290d6c051743ec51c650b955b'

def sha(data):return hashlib.sha256(data).hexdigest()

def require(ok,message):
    if not ok:raise ValueError(message)

def boot_entry(sd,system):
    """Detecta el nombre existente por sus opciones; nunca edita el menú."""
    config=configparser.ConfigParser(interpolation=None,strict=True)
    config.read_string((Path(sd)/'bootloader/hekate_ipl.ini').read_text(encoding='utf-8-sig'))
    found=[]
    for name in config.sections():
        section=config[name]
        if system=='sysMMC':
            matches=section.get('stock')=='1' and section.get('emummc_force_disable')=='1' and 'payload' not in section
        elif system=='emuMMC':
            matches=(section.get('emummcforce')=='1' and section.get('stock','0')=='0' and section.get('emummc_force_disable','0')=='0') or (section.get('payload')=='bootloader/payloads/fusee.bin' and set(section)<= {'payload','icon'})
        else:raise ValueError('Sistema no válido')
        if matches:found.append(name)
    require(len(found)==1,'Indica el nombre exacto de la entrada '+system+' en Hekate; no pude identificar una sola entrada')
    return found[0]

def safe(sd,name):
    p=sd/name
    require(p.resolve().is_relative_to(sd),'Ruta fuera de la SD')
    require(not p.exists() or p.is_file(),'Se esperaba un archivo: '+name)
    for q in (p,*p.parents):
        require(not q.is_symlink(),'No se admiten enlaces simbólicos')
        if q==sd:break
    return p

def valid_wav(p):
    return validate_wav(p)

def audio_selection(data,name):
    pattern=rb'(?m)^[ \t]*audio_file[ \t]*=[^\r\n]*'
    require(len(re.findall(pattern,data))<=1,'audio_file duplicado en config.ini')
    line=b'audio_file='+name.encode('ascii')
    if re.search(pattern,data):return re.sub(pattern,line,data)
    nl=b'\r\n' if b'\r\n' in data else b'\n'
    return data+(nl if data and not data.endswith(b'\n') else b'')+line+nl

def logo_config(data,entry):
    require(entry and all(c not in entry for c in '\r\n[]'),'Nombre de entrada no válido')
    pattern=rb'(?m)^\['+re.escape(entry.encode('utf-8'))+rb'\][ \t]*\r?\n'
    matches=list(re.finditer(pattern,data));require(len(matches)==1,'Entrada inexistente o duplicada')
    m=matches[0];tail=re.search(rb'(?m)^\[',data[m.end():]);end=m.end()+tail.start() if tail else len(data)
    block=data[m.end():end];nl=b'\r\n' if m[0].endswith(b'\r\n') else b'\n'
    line=b'logopath=bootloader/res/jherald_bootlogo.bmp'+nl
    require(len(re.findall(rb'(?m)^\s*logopath\s*=',block))<=1,'logopath duplicado')
    if re.search(rb'(?m)^\s*logopath\s*=',block):
        block=re.sub(rb'(?m)^[ \t]*logopath[ \t]*=[^\r\n]*(?:\r?\n|$)',line,block)
    else:block=line+block
    return data[:m.end()]+block+data[end:]

def plan(sd,component,clip='yoshi',entry=None,replace_config=False,custom_wav=None):
    sd=Path(sd).resolve()
    require(sd.is_dir() and (sd/'bootloader/hekate_ipl.ini').is_file() and (sd/'atmosphere/package3').is_file(),'Selecciona la raíz de una SD ya configurada con Hekate y Atmosphère')
    files={}
    if component=='audio':
        src=ROOT/'components/bootsound'
        binary=(src/'release'/MODULE/'exefs.nsp').read_bytes()
        require(sha(binary)==NSP_SHA,'El módulo distribuido no coincide con su hash')
        files[MODULE+'/exefs.nsp']=binary;files[MODULE+'/flags/boot2.flag']=b''
        name='custom_sound.wav' if custom_wav else 'yoshi_sound.wav' if clip=='yoshi' else 'startup_example.wav'
        wav=Path(custom_wav) if custom_wav else src/('audio/yoshi_sound.wav' if clip=='yoshi' else 'package/startup_example.wav')
        valid_wav(wav);files[CONFIG+'/'+name]=wav.read_bytes()
        fallback=src/'audio/startup.wav';valid_wav(fallback)
        if not safe(sd,CONFIG+'/startup.wav').exists():files[CONFIG+'/startup.wav']=fallback.read_bytes()
        ini=safe(sd,CONFIG+'/config.ini')
        if custom_wav or replace_config or not ini.exists():
            files[CONFIG+'/config.ini']=audio_selection(ini.read_bytes() if ini.exists() else b'# BootSound\n',name)
    elif component=='logo':
        if entry is None:entry=boot_entry(sd,'emuMMC')
        files['bootloader/res/jherald_bootlogo.bmp']=(ROOT/'components/bootsplash/logos/jherald_bootlogo.bmp').read_bytes()
        ini='bootloader/hekate_ipl.ini';files[ini]=logo_config(safe(sd,ini).read_bytes(),entry)
    else:raise ValueError('Componente no válido')
    before={name:safe(sd,name).read_bytes() if safe(sd,name).is_file() else None for name in files}
    return sd,files,before

def apply(sd,files,before,backup):
    backup=Path(backup).resolve()
    require(not backup.exists() and not backup.is_relative_to(sd),'Usa una carpeta NUEVA de respaldo fuera de la SD')
    require(not backup.is_relative_to(ROOT),'Guarda el respaldo fuera del repositorio')
    # Verify every target before the first mutation; backups contain only known paths.
    for name,old in before.items():
        p=safe(sd,name)
        require((p.read_bytes() if p.is_file() else None)==old,'La SD cambió: vuelve a revisar el plan')
        require(not p.with_name(p.name+'.jherald-new').exists(),'Archivo temporal previo: revisar antes de instalar')
    backup.mkdir(parents=True)
    manifest={}
    for name,old in before.items():
        manifest[name]=sha(old) if old is not None else None
        if old is not None:
            p=backup/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(old)
            require(p.read_bytes()==old,'Respaldo no verificado')
    (backup/'BACKUP.json').write_text(json.dumps({'before':manifest,'installed':{n:sha(d) for n,d in files.items()}},indent=2))
    for name,data in files.items():
        p=safe(sd,name)
        require((p.read_bytes() if p.is_file() else None)==before[name],'La SD cambió durante el respaldo')
        p.parent.mkdir(parents=True,exist_ok=True);tmp=p.with_name(p.name+'.jherald-new')
        with tmp.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        require(tmp.read_bytes()==data,'Lectura del archivo temporal falló')
        os.replace(tmp,p);require(p.read_bytes()==data,'Verificación de instalación falló')
    print('Instalación verificada. Respaldo:',backup)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('component',choices=['audio','logo']);p.add_argument('--sd',required=True)
    sound=p.add_mutually_exclusive_group();sound.add_argument('--clip',choices=['yoshi','ejemplo']);sound.add_argument('--audio-file',type=Path,help='Audio propio para convertir');sound.add_argument('--custom-wav',type=Path,help='WAV propio ya convertido')
    p.add_argument('--start',type=float,default=0);p.add_argument('--duration',type=float,default=10);p.add_argument('--download-ffmpeg',action='store_true')
    p.add_argument('--entry',help='Nombre exacto de la entrada emuMMC; por defecto se detecta');p.add_argument('--replace-config',action='store_true');p.add_argument('--apply',action='store_true');p.add_argument('--backup-dir');a=p.parse_args()
    require(a.component=='audio' or not (a.audio_file or a.custom_wav),'El audio propio requiere el componente audio')
    with tempfile.TemporaryDirectory(prefix='bootsound-install-') as temp:
        custom=a.custom_wav
        if a.audio_file:
            custom=Path(temp)/'custom_sound.wav'
            info=convert_audio(a.audio_file,custom,a.start,a.duration,download=a.download_ffmpeg)
            print(f"Audio convertido y verificado: {info['seconds']:.2f} segundos")
        sd,files,before=plan(a.sd,a.component,a.clip or 'yoshi',a.entry,a.replace_config,custom)
        for name,data in files.items():print(('Sin cambio' if before[name]==data else 'Reemplazar' if before[name] is not None else 'Añadir')+': '+name)
        if not a.apply:print('Solo revisión. No se escribió en la SD.');return
        require(a.backup_dir,'Falta --backup-dir para aplicar');apply(sd,files,before,a.backup_dir)

if __name__=='__main__':main()
