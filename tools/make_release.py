"""Empaqueta solo archivos registrados en Git. Nunca agrega output ni claves."""
from pathlib import Path
import argparse, hashlib, io, json, subprocess, tarfile, zipfile

ROOT=Path(__file__).resolve().parents[1]
FORBIDDEN={'prod.keys','title.keys','bluetooth_devices.db','history.bin','history.next','history.prev','transaction.bin','auto_binding.h','source-pairings.bts','hack-pairings.bth'}

def tracked():
    data=subprocess.check_output(['git','ls-files','-z'],cwd=ROOT)
    names=[n for n in data.decode('utf-8').split('\0') if n]
    if not names:raise ValueError('No hay archivos registrados en Git')
    for name in names:
        p=Path(name)
        if p.name in FORBIDDEN or p.suffix=='.keys' or any(part in ('output','build','backups','__pycache__','.git') for part in p.parts):
            raise ValueError('Archivo privado/generado registrado: '+name)
    return names

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',required=True);a=p.parse_args();out=Path(a.out).resolve()
    if out.exists() or out.is_relative_to(ROOT):raise ValueError('Usa un ZIP nuevo fuera del repositorio')
    for args in (['diff','--quiet','HEAD','--'],['diff','--cached','--quiet']):
        if subprocess.run(['git',*args],cwd=ROOT).returncode:raise ValueError('Confirma primero todos los cambios registrados')
    names=set(tracked())
    archive=subprocess.check_output(['git','archive','--format=tar','HEAD'],cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(archive),mode='r:') as tar:
        entries={item.name:tar.extractfile(item).read() for item in tar.getmembers() if item.isfile()}
    if set(entries)!=names:raise ValueError('El índice y el commit no contienen los mismos archivos')
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in entries.items()}
    entries['SHA256-MANIFEST.json']=(json.dumps(hashes,indent=2)+'\n').encode()
    out.parent.mkdir(parents=True,exist_ok=True);prefix='Nintendo-Switch-BT-Sound-v1.0.2/'
    with zipfile.ZipFile(out,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,data in entries.items():z.writestr(prefix+name,data)
    with zipfile.ZipFile(out) as z:
        if z.testzip():raise ValueError('Error de integridad ZIP')
        for name,digest in hashes.items():
            if hashlib.sha256(z.read(prefix+name)).hexdigest()!=digest:raise ValueError('Hash ZIP distinto: '+name)
    print('ZIP verificado:',len(entries),'archivos; SHA256',hashlib.sha256(out.read_bytes()).hexdigest())

if __name__=='__main__':main()
