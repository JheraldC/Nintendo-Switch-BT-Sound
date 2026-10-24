"""Genera una copia local de package3 y verifica la región y los píxeles."""
from pathlib import Path
import argparse, hashlib
from PIL import Image

START=0x400000
END=0x7c0000
SIZE=0x800000

def patch(base,image):
    if len(base)!=SIZE or base[:4]!=b'PK31':raise ValueError('Se necesita package3 PK31 de 8 MiB')
    if image.size!=(1280,720):raise ValueError('La imagen debe medir 1280x720')
    rotated=image.convert('RGBA').transpose(Image.Transpose.ROTATE_90)
    pixels=rotated.tobytes('raw','BGRA')
    region=bytearray(END-START)
    for row in range(1280):region[row*768*4:row*768*4+720*4]=pixels[row*720*4:(row+1)*720*4]
    result=base[:START]+bytes(region)+base[END:]
    if result[:START]!=base[:START] or result[END:]!=base[END:]:raise ValueError('Cambio fuera del splash')
    actual=b''.join(result[START+row*768*4:START+row*768*4+720*4] for row in range(1280))
    inverse=Image.frombytes('RGBA',(720,1280),actual,'raw','BGRA').transpose(Image.Transpose.ROTATE_270)
    if inverse.tobytes()!=image.convert('RGBA').tobytes():raise ValueError('Los píxeles no coinciden')
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--base',required=True);p.add_argument('--image',required=True);p.add_argument('--out',required=True);a=p.parse_args()
    base=Path(a.base).resolve();out=Path(a.out).resolve()
    if base==out or out.exists():raise ValueError('La salida debe ser un archivo NUEVO')
    if out.drive and out.drive.lower()!=Path(__file__).resolve().drive.lower():
        raise ValueError('Guarda la salida en el disco del proyecto, no en una SD')
    data=base.read_bytes()
    with Image.open(a.image) as img:result=patch(data,img)
    with out.open('xb') as f:f.write(result)
    if out.read_bytes()!=result:raise ValueError('No coincide la lectura de salida')
    print('Copia local verificada. SHA256:',hashlib.sha256(result).hexdigest())

if __name__=='__main__':main()
