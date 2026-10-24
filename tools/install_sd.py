"""Instalación limitada a sonido o logo. Sin --apply solo muestra el plan."""
from pathlib import Path
import argparse, hashlib, json, os, re, wave

ROOT=Path(__file__).resolve().parents[1]
MODULE='atmosphere/contents/42000000004A4253'
CONFIG='config/JheraldBootSound'
NSP_SHA='8b995212658bd8dae1526525b0b8022a728b423c233c7809a88482fc50f5f019'

def sha(data):return hashlib.sha256(data).hexdigest()

def require(ok,message):
    if not ok:raise ValueError(message)

def safe(sd,name):
    p=sd/name
    require(p.resolve().is_relative_to(sd),'Ruta fuera de la SD')
    require(not p.exists() or p.is_file(),'Se esperaba un archivo: '+name)
    for q in (p,*p.parents):
        require(not q.is_symlink(),'No se admiten enlaces simbólicos')
        if q==sd:break
    return p

def valid_wav(p):
    with wave.open(str(p),'rb') as f:
        require((f.getnchannels(),f.getsampwidth(),f.getframerate(),f.getcomptype())==(2,2,48000,'NONE'),'WAV debe ser PCM 16-bit, estéreo, 48000 Hz')
        require(0<f.getnframes()*4<=2*1024*1024,'Audio fuera del límite de 2 MiB PCM')
        require(len(f.readframes(f.getnframes()))==f.getnframes()*4,'WAV truncado')

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

def plan(sd,component,clip='yoshi',entry='Sistema Hack',replace_config=False):
    sd=Path(sd).resolve()
    require(sd.is_dir() and (sd/'bootloader/hekate_ipl.ini').is_file() and (sd/'atmosphere/package3').is_file(),'Selecciona la raíz de una SD ya configurada con Hekate y Atmosphère')
    files={}
    if component=='audio':
        src=ROOT/'components/bootsound'
        binary=(src/'release'/MODULE/'exefs.nsp').read_bytes()
        require(sha(binary)==NSP_SHA,'El módulo distribuido no coincide con su hash')
        files[MODULE+'/exefs.nsp']=binary;files[MODULE+'/flags/boot2.flag']=b''
        name='yoshi_sound.wav' if clip=='yoshi' else 'startup_example.wav'
        wav=src/('audio/yoshi_sound.wav' if clip=='yoshi' else 'package/startup_example.wav')
        valid_wav(wav);files[CONFIG+'/'+name]=wav.read_bytes()
        fallback=src/'audio/startup.wav';valid_wav(fallback)
        if not safe(sd,CONFIG+'/startup.wav').exists():files[CONFIG+'/startup.wav']=fallback.read_bytes()
        if replace_config or not safe(sd,CONFIG+'/config.ini').exists():
            files[CONFIG+'/config.ini']=('# BootSound\naudio_file='+name+'\n').encode()
    elif component=='logo':
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
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('component',choices=['audio','logo']);p.add_argument('--sd',required=True);p.add_argument('--clip',choices=['yoshi','ejemplo'],default='yoshi');p.add_argument('--entry',default='Sistema Hack');p.add_argument('--replace-config',action='store_true');p.add_argument('--apply',action='store_true');p.add_argument('--backup-dir');a=p.parse_args()
    sd,files,before=plan(a.sd,a.component,a.clip,a.entry,a.replace_config)
    for name,data in files.items():print(('Sin cambio' if before[name]==data else 'Reemplazar' if before[name] is not None else 'Añadir')+': '+name)
    if not a.apply:print('Solo revisión. No se escribió en la SD.');return
    require(a.backup_dir,'Falta --backup-dir para aplicar');apply(sd,files,before,a.backup_dir)

if __name__=='__main__':main()
