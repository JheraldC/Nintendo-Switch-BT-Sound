"""Conversión local al WAV de BootSound. Solo usa la biblioteca estándar."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import platform
import shutil
import struct
import subprocess
import tempfile
import urllib.request
import zipfile
import sys
if str(Path(__file__).resolve().parents[1]) not in sys.path:
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.portable_runtime import manifest,platform_key,cache_root,download as download_asset

MAX_PCM = 2 * 1024 * 1024
MAX_SECONDS = 10.0
RUNTIME_VERSION = 'imageio-ffmpeg-0.8.1-win64'
WHEEL_URL = 'https://files.pythonhosted.org/packages/82/0c/a1d49f3dd2688bd750b4986f8e271ad11f8e729a896070ea25f587b44da8/imageio_ffmpeg-0.8.1-py3-none-win_amd64.whl'
WHEEL_SHA256 = 'f27ca4cb85e2d4946deedbce1a81245dfd942a2bcf94b53676fb3956aa95bf1b'
EXE_MEMBER = 'imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v9.0.2.exe'
EXE_SHA256 = '3256173f3f8bffd7df12227c68adf68025edb1832273a9530688a7bb1ed8edec'
LICENSE_MEMBER = 'imageio_ffmpeg-0.8.1.dist-info/licenses/LICENSE'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def validate_wav(path):
    """Valida los límites y campos PCM que exige el lector C de la consola."""
    path = Path(path)
    require(path.is_file() and 44 <= path.stat().st_size <= MAX_PCM + 0x10000,
            'El WAV está vacío o supera el tamaño admitido por BootSound')
    data = path.read_bytes()
    require(data[:4] == b'RIFF' and data[8:12] == b'WAVE', 'No es un WAV RIFF')
    end = struct.unpack_from('<I', data, 4)[0] + 8
    require(12 <= end <= len(data), 'WAV truncado')
    pos, fmt, pcm_size = 12, None, None
    while pos + 8 <= end:
        kind, size = struct.unpack_from('<4sI', data, pos)
        start = pos + 8
        require(start + size <= end, 'Chunk WAV truncado')
        if kind == b'fmt ':
            require(fmt is None and size >= 16, 'Formato WAV duplicado o incompleto')
            fmt = struct.unpack_from('<HHIIHH', data, start)
        elif kind == b'data':
            require(pcm_size is None, 'Datos WAV duplicados')
            pcm_size = size
        pos = start + size + size % 2
    require(fmt == (1, 2, 48000, 192000, 4, 16),
            'WAV debe ser PCM de 16 bits, estéreo, 48000 Hz, con alineación válida')
    require(pcm_size is not None and 0 < pcm_size <= MAX_PCM and pcm_size % 4 == 0,
            'Audio fuera del límite de 2 MiB PCM o incompleto')
    return {'seconds': pcm_size / 192000, 'pcm_bytes': pcm_size,
            'sample_rate': 48000, 'channels': 2, 'bits': 16}


def run_process(args, **kwargs):
    return subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True,
                          creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), **kwargs)


def checked_executable(path):
    try:
        result = run_process([str(path), '-version'], timeout=15)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError('No se pudo ejecutar FFmpeg: ' + str(path)) from exc
    require(result.returncode == 0 and result.stdout.startswith(b'ffmpeg version '),
            'El ejecutable no es un FFmpeg válido: ' + str(path))
    return str(path)


def runtime_asset():return manifest()[platform_key()]['ffmpeg']

def runtime_directory():
    return cache_root()/'runtime'/('ffmpeg-0.8.1-'+platform_key())


def extract_runtime(wheel, destination, asset=None):
    """Extrae solo FFmpeg y los avisos. No instala imageio ni otras librerías."""
    wheel, destination = Path(wheel), Path(destination)
    asset=asset or runtime_asset()
    require(hashlib.sha256(wheel.read_bytes()).hexdigest() == asset['sha256'],
            'El SHA-256 de la descarga de FFmpeg no coincide')
    with zipfile.ZipFile(wheel) as archive:
        binary = archive.read(asset['member'])
        require(hashlib.sha256(binary).hexdigest() == asset['executable_sha256'],
                'El SHA-256 del ejecutable FFmpeg no coincide')
        license_text = archive.read(asset['license_member'])
    destination.mkdir(parents=True, exist_ok=True)
    # All archive names are fixed; no extractall or archive-controlled paths.
    exe_name='ffmpeg.exe' if os.name=='nt' else 'ffmpeg'
    (destination / exe_name).write_bytes(binary)
    (destination / exe_name).chmod(0o755)
    (destination / 'IMAGEIO-LICENSE.txt').write_bytes(license_text)
    executable = checked_executable(destination / exe_name)
    result = run_process([executable, '-L'], timeout=15)
    require(result.returncode == 0, 'No se pudo obtener la licencia de FFmpeg')
    (destination / 'FFMPEG-LICENSE.txt').write_bytes(result.stdout + result.stderr)
    (destination / 'ORIGIN.json').write_text(json.dumps({
        'distribution': asset['version'], 'url': asset['url'],
        'wheel_sha256': asset['sha256'], 'executable_sha256': asset['executable_sha256'],
        'ffmpeg_sources': 'https://github.com/FFmpeg/FFmpeg',
        'binary_member':asset['member'],
        'distribution_sources': 'https://github.com/imageio/imageio-ffmpeg',
    }, indent=2), encoding='utf-8')
    return Path(executable)


def download_runtime(destination):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    asset=runtime_asset()
    print('Preparando FFmpeg portátil, solo la primera vez...')
    with tempfile.TemporaryDirectory(prefix='ffmpeg-', dir=destination.parent) as temp:
        temp = Path(temp)
        wheel = temp / 'runtime.whl'
        download_asset(asset,wheel)
        prepared = temp / 'prepared'
        extract_runtime(wheel, prepared,asset)
        destination.mkdir(parents=True, exist_ok=True)
        exe_name='ffmpeg.exe' if os.name=='nt' else 'ffmpeg'
        for name in (exe_name, 'IMAGEIO-LICENSE.txt', 'FFMPEG-LICENSE.txt', 'ORIGIN.json'):
            os.replace(prepared / name, destination / name)
    return checked_executable(destination / exe_name)


def find_ffmpeg(download=False):
    explicit = os.environ.get('FFMPEG_BINARY')
    if explicit:
        return checked_executable(explicit)
    system = shutil.which('ffmpeg')
    if system:
        return checked_executable(system)
    cache = runtime_directory()
    executable = cache / ('ffmpeg.exe' if os.name=='nt' else 'ffmpeg')
    if executable.is_file() and hashlib.sha256(executable.read_bytes()).hexdigest() == runtime_asset()['executable_sha256']:
        return checked_executable(executable)
    if download:
        return download_runtime(cache)
    raise ValueError('No encuentro FFmpeg. Usa --download-ffmpeg o instala FFmpeg en PATH.')


def convert_audio(source, output, start=0.0, duration=MAX_SECONDS, ffmpeg=None, download=False):
    source, output = Path(source).expanduser().resolve(), Path(output).expanduser().resolve()
    require(source.is_file(), 'El audio de origen no existe')
    require(source != output and not output.exists(), 'La salida debe ser un archivo nuevo, distinto del original')
    require(math.isfinite(start) and start >= 0, 'El inicio debe ser un número mayor o igual a cero')
    require(math.isfinite(duration) and 0 < duration <= MAX_SECONDS, 'La duración debe estar entre 0 y 10 segundos')
    # A compatible short WAV needs neither FFmpeg nor a download.
    compatible = None
    try:
        compatible = validate_wav(source)
    except (ValueError, OSError, struct.error):
        pass
    if compatible and start == 0 and compatible['seconds'] <= duration:
        data = source.read_bytes()
    else:
        executable = checked_executable(ffmpeg) if ffmpeg else find_ffmpeg(download)
        with tempfile.TemporaryDirectory(prefix='bootsound-audio-') as temp:
            converted = Path(temp) / 'converted.wav'
            args = [executable, '-nostdin', '-hide_banner', '-loglevel', 'error', '-n',
                    '-protocol_whitelist', 'file,pipe', '-ss', str(start), '-i', str(source),
                    '-map', '0:a:0', '-vn', '-sn', '-dn', '-t', str(duration),
                    '-ac', '2', '-ar', '48000', '-c:a', 'pcm_s16le',
                    '-fflags', '+bitexact', '-flags:a', '+bitexact', '-f', 'wav', str(converted)]
            try:
                result = run_process(args, timeout=120)
            except subprocess.TimeoutExpired as exc:
                raise ValueError('La conversión tardó demasiado; prueba con otro archivo o fragmento') from exc
            require(result.returncode == 0, 'No se pudo convertir el audio: ' +
                    result.stderr.decode('utf-8', errors='replace')[-1500:])
            validate_wav(converted)
            data = converted.read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as target:
        target.write(data)
    return validate_wav(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--start', type=float, default=0)
    parser.add_argument('--duration', type=float, default=MAX_SECONDS)
    parser.add_argument('--download-ffmpeg', action='store_true')
    args = parser.parse_args()
    try:
        info = convert_audio(args.source, args.output, args.start, args.duration, download=args.download_ffmpeg)
    except (ValueError, OSError) as exc:
        parser.exit(1, str(exc) + '\n')
    print(f"WAV verificado: {info['seconds']:.2f} s, PCM 16-bit, estéreo, 48000 Hz. {args.output}")


if __name__ == '__main__':
    main()
