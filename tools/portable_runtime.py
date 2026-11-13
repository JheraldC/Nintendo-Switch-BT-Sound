"""Dependencias portátiles verificadas; sin administrador ni paquetes del sistema."""
from pathlib import Path,PurePosixPath
import hashlib,json,os,platform,shutil,subprocess,sys,tarfile,tempfile,urllib.request,zipfile

ROOT=Path(__file__).resolve().parents[1]

def platform_key():
    machine=platform.machine().lower()
    arch={'amd64':'x86_64','x86_64':'x86_64','arm64':'aarch64','aarch64':'aarch64'}.get(machine)
    system='windows' if os.name=='nt' else sys.platform
    key=f'{system}-{arch}'
    if key not in manifest():raise ValueError('Plataforma no disponible: '+key)
    return key

def manifest():return json.loads((ROOT/'tools/runtime-manifest.json').read_text(encoding='utf-8'))

def cache_root():
    if os.environ.get('BT_SOUND_CACHE'):return Path(os.environ['BT_SOUND_CACHE']).expanduser().resolve()
    # Short path also avoids Microsoft Store Python's long AppData redirection.
    return Path.home()/'.bt-sound'

def download(asset,path):
    path=Path(path);digest=hashlib.sha256();size=0
    print(f"Descargando {asset['url'].rsplit('/',1)[-1]} ({asset['bytes']/1024/1024:.1f} MiB)...")
    try:
        with urllib.request.urlopen(asset['url'],timeout=60) as response,path.open('xb') as target:
            while block:=response.read(1024*1024):
                size+=len(block)
                if size>asset['bytes']:raise ValueError('La descarga supera el tamaño esperado')
                digest.update(block);target.write(block)
    except OSError as exc:raise ValueError('No se pudo descargar la dependencia. Revisa la conexión a Internet.') from exc
    if size!=asset['bytes'] or digest.hexdigest()!=asset['sha256']:raise ValueError('SHA-256 o tamaño de descarga incorrecto')

def extract_archive(archive,destination,include=None):
    destination=Path(destination).resolve();destination.mkdir(parents=True,exist_ok=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as z:
            for member in z.infolist():
                if not (destination/member.filename).resolve().is_relative_to(destination):raise ValueError('Ruta ZIP fuera del destino')
            for member in z.infolist():
                if include is None or include(member.filename):z.extract(member,destination)
    else:
        with tarfile.open(archive,'r:*') as tar:
            for member in tar.getmembers():
                target=(destination/member.name).resolve()
                if not target.is_relative_to(destination) or member.isdev() or member.isfifo():raise ValueError('Entrada TAR no admitida')
                if member.issym() or member.islnk():
                    link=(target.parent if member.issym() else destination)/member.linkname
                    if not link.resolve().is_relative_to(destination):raise ValueError('Enlace TAR fuera del destino')
            members=[m for m in tar.getmembers() if include is None or include(m.name)]
            if hasattr(tarfile,'data_filter'):tar.extractall(destination,members=members,filter='data')
            else:tar.extractall(destination,members=members)

def ensure_arm_toolchain(force_portable=False):
    candidates=[]
    if os.environ.get('DEVKITARM'):candidates.append(Path(os.environ['DEVKITARM']))
    if os.name=='nt':candidates.append(Path('C:/devkitPro/devkitARM'))
    found=shutil.which('arm-none-eabi-gcc')
    if found:candidates.append(Path(found).resolve().parent.parent)
    suffix='.exe' if os.name=='nt' else ''
    for candidate in ([] if force_portable else candidates):
        if all((candidate/'bin'/('arm-none-eabi-'+t+suffix)).is_file() for t in ('gcc','objcopy','nm','size','objdump')):
            os.environ['DEVKITARM']=str(candidate);return candidate
    asset=manifest()[platform_key()]['arm']
    destination=cache_root()/'runtime'/('arm-'+asset['sha256'][:12])
    candidate=destination/asset['root']
    marker=destination/'VERIFIED.json'
    if not marker.is_file() or json.loads(marker.read_text()).get('archive_sha256')!=asset['sha256']:
        destination.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='arm-',dir=destination.parent) as temp:
            temp=Path(temp);archive=temp/'toolchain.archive';download(asset,archive)
            def needed(name):
                parts=PurePosixPath(name).parts
                return len(parts)<=1 or parts[1] in ('bin','lib','libexec','arm-none-eabi','distro-info') or (len(parts)>2 and parts[1:3]==('share','licenses')) or (len(parts)==2 and not '.'==parts[1])
            extract_archive(archive,temp/'extracted',include=needed)
            if not (temp/'extracted'/asset['root']/'bin'/('arm-none-eabi-gcc'+suffix)).is_file():raise ValueError('Compilador ARM ausente del paquete')
            # Versioned cache: never install over an arbitrary existing directory.
            if destination.exists():raise ValueError('Caché ARM incompleta; revisa '+str(destination))
            os.replace(temp/'extracted',destination)
            marker.write_text(json.dumps({'archive_sha256':asset['sha256'],'url':asset['url']}))
    if not (candidate/'bin'/('arm-none-eabi-gcc'+suffix)).is_file():raise ValueError('Caché ARM dañada: '+str(destination))
    os.environ['DEVKITARM']=str(candidate)
    return candidate
