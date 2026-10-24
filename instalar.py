"""Asistente local. No envía claves ni archivos de la consola a Internet."""
from pathlib import Path
import subprocess, sys

ROOT=Path(__file__).resolve().parent

def run(args,cwd=ROOT):
    subprocess.run([sys.executable,*map(str,args)],cwd=cwd,check=True)

def main():
    print('Jherald Switch Tools\n1. Sonido al arrancar Hack\n2. Imagen de Hekate\n3. Preparar payload Bluetooth para esta SD')
    choice=input('Elige 1, 2 o 3: ').strip()
    if choice not in ('1','2','3'):raise ValueError('Opción no válida')
    sd=Path(input('Letra de la microSD (por ejemplo E:): ').strip().rstrip('/\\')+'/').resolve()
    backup=Path.home()/'Documents/SwitchBackups'
    from datetime import datetime
    backup=backup/('jherald-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    if choice in ('1','2'):
        args=['tools/install_sd.py','audio' if choice=='1' else 'logo','--sd',sd]
        if choice=='1':
            args+=['--clip','yoshi' if input('Sonido: 1 Yoshi / 2 ejemplo generado [1]: ').strip()!='2' else 'ejemplo']
            print('Si ya tienes una selección de sonido, se conserva.')
        else:args+=['--entry',input('Nombre exacto de la entrada Hack [Sistema Hack]: ').strip() or 'Sistema Hack']
        run(args)
        if input('Escribe INSTALAR para aplicar este plan con copia de seguridad: ').strip()=='INSTALAR':
            run([*args,'--apply','--backup-dir',backup])
        return
    bt=ROOT/'components/bluetooth-sync'
    original=input('Nombre de la entrada Original [Sistema Original]: ').strip() or 'Sistema Original'
    hack=input('Nombre de la entrada Hack [Sistema Hack]: ').strip() or 'Sistema Hack'
    profile=input('Firmware del Sistema Original (solo 23.0.1): ').strip()
    print('Necesitas Python, devkitARM y WSL con gcc/make/python3. Puede tardar varios minutos.')
    print('La base externa debe estar activada y creada al arrancar Hack una vez; consulta docs/BLUETOOTH.md.')
    run(['prepare.py','--sd',sd,'--original',original,'--hack',hack,'--firmware-profile',profile],bt)
    for command in (['build_payload.py'],['tests/test_portable.py'],['tests/test_result_paths.py'],['run_host_tests.py'],['audit_auto.py'],['deploy_portable.py','--sd',sd]):
        run(command,bt)
    print('Se añadirán cuatro archivos. El menú y la base actual se conservan. No se ejecuta la sincronización ahora.')
    if input('Escribe INSTALAR para guardar el respaldo privado e instalar el payload: ').strip()=='INSTALAR':
        run(['deploy_portable.py','--sd',sd,'--apply','--backup-dir',backup],bt)
        print('Después: Hekate > Payloads > BT-Sync-Una-Vez.bin. Comprueba OK y la reconexión en Hack.')

if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as exc:
        print('No se pudo completar:',exc,file=sys.stderr);sys.exit(1)
