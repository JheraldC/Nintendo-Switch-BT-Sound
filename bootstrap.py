"""Descarga verificada y ejecución local de una revisión fija del asistente."""
from pathlib import Path
import hashlib,json,os,stat,subprocess,sys,tempfile,urllib.request,zipfile

COMMIT='v1.2.0'
ARCHIVE_SHA256='cf6bd5017d58cd422cd7da7b92f1e739ec669e7cc59f0707896efdfad409bdbc'
ARCHIVE_BYTES=8908560
URL='https://github.com/JheraldC/Nintendo-Switch-BT-Sound/releases/download/v1.2.0/installer-source-v1.2.0.zip'

def cache_root():return Path(os.environ.get('BT_SOUND_CACHE') or Path.home()/'.bt-sound').expanduser().resolve()

def verified_project(cache):
    cache=Path(cache).resolve()
    directory=cache/'source'/COMMIT[:12]
    marker=directory/'BOOTSTRAP-SHA256.json'
    if marker.is_file():
        proof=json.loads(marker.read_text(encoding='utf-8'))
        if proof.get('archive_sha256')!=ARCHIVE_SHA256:raise ValueError('La copia local corresponde a otro paquete')
        for name,digest in proof['files'].items():
            p=directory/name
            if not p.resolve().is_relative_to(directory) or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:
                raise ValueError('La copia local cambió; revisa '+str(directory))
        return directory
    if directory.exists():raise ValueError('Hay una descarga incompleta; revisa '+str(directory))
    directory.parent.mkdir(parents=True,exist_ok=True)
    print('Descargando Nintendo-Switch-BT-Sound 1.2.0, revisión '+COMMIT[:7]+'...')
    with tempfile.TemporaryDirectory(prefix='project-',dir=directory.parent) as temp:
        temp=Path(temp).resolve();archive=temp/'project.zip';digest=hashlib.sha256();size=0
        with urllib.request.urlopen(URL,timeout=60) as response,archive.open('xb') as target:
            while block:=response.read(1024*1024):
                size+=len(block)
                if size>ARCHIVE_BYTES:raise ValueError('Tamaño del paquete inesperado')
                digest.update(block);target.write(block)
        if size!=ARCHIVE_BYTES or digest.hexdigest()!=ARCHIVE_SHA256:raise ValueError('El SHA-256 del proyecto no coincide')
        prefix='Nintendo-Switch-BT-Sound-'+COMMIT+'/'
        files={};prepared=temp/'prepared';prepared.mkdir()
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                if not entry.filename.startswith(prefix):raise ValueError('Raíz del paquete no válida')
                if entry.is_dir():continue
                if stat.S_ISLNK(entry.external_attr>>16):raise ValueError('Enlace no permitido en el proyecto')
                name=entry.filename[len(prefix):];p=prepared/name
                if not p.resolve().is_relative_to(prepared):raise ValueError('Ruta fuera del proyecto')
                data=z.read(entry);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
                files[name]=hashlib.sha256(data).hexdigest()
        (prepared/'BOOTSTRAP-SHA256.json').write_text(json.dumps({'commit':COMMIT,'archive_sha256':ARCHIVE_SHA256,'files':files},indent=2),encoding='utf-8')
        os.replace(prepared,directory)
    return directory

def main():
    cache=cache_root();cache.mkdir(parents=True,exist_ok=True)
    lock=cache/'installer.lock'
    try:
        with lock.open('x') as f:f.write(str(os.getpid()))
    except FileExistsError:raise ValueError('Ya hay un instalador abierto o una ejecución interrumpida. Revisa '+str(lock))
    try:
        project=verified_project(cache)
        return subprocess.call([sys.executable,str(project/'instalar.py'),*sys.argv[1:]],cwd=project)
    finally:lock.unlink()

if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError) as exc:print('No se pudo iniciar:',exc,file=sys.stderr);raise SystemExit(1)
