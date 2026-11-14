"""Asistente local para Windows, macOS y Linux. Los datos de la SD no se suben."""
from pathlib import Path
import argparse,re,subprocess,sys,tempfile
from datetime import datetime

ROOT=Path(__file__).resolve().parent

def choose_entry(sd,system):
    from tools.install_sd import boot_entry
    try:default=boot_entry(sd,system)
    except ValueError:default=''
    hint=(' ['+default+']') if default else ''
    selected=input('Nombre exacto de la entrada '+system+' en Hekate'+hint+': ').strip() or default
    if not selected:raise ValueError('Debes indicar el nombre de la entrada '+system)
    return selected

def run(args,cwd=ROOT):
    subprocess.run([sys.executable,*map(str,args)],cwd=cwd,check=True)

def install_files(sd,component,backup,**kwargs):
    from tools.install_sd import plan,apply
    actual,files,before=plan(sd,component,**kwargs)
    for name,data in files.items():
        print(('Sin cambio' if before[name]==data else 'Reemplazar' if before[name] is not None else 'Añadir')+': '+name)
    print('Respaldo de los archivos anteriores:',backup)
    if input('Escribe INSTALAR para aplicar este plan: ').strip()=='INSTALAR':
        apply(actual,files,before,backup)
    else:print('Cancelado. No se escribió en la SD.')

def audio_flow(sd,backup):
    selected=input('Sonido: 1 Yoshi / 2 ejemplo generado / 3 elegir mi archivo [1]: ').strip() or '1'
    if selected not in ('1','2','3'):raise ValueError('Opción de sonido no válida')
    if selected!='3':
        replace=input('¿Activar este sonido en lugar del seleccionado actualmente? [s/N]: ').strip().lower()=='s'
        install_files(sd,'audio',backup,clip='yoshi' if selected=='1' else 'ejemplo',replace_config=replace)
        return
    from tools.file_picker import select_audio_file
    source=select_audio_file()
    if not source:
        print('Selección cancelada. No se escribió en la SD.');return
    start=float(input('Segundo de inicio [0]: ').strip() or '0')
    duration=float(input('Duración del fragmento, hasta 10 segundos [10]: ').strip() or '10')
    from tools.audio_convert import convert_audio
    with tempfile.TemporaryDirectory(prefix='bootsound-selected-') as temp:
        converted=Path(temp)/'custom_sound.wav'
        info=convert_audio(source,converted,start,duration,download=True)
        print(f"Audio listo: {info['seconds']:.2f} s, WAV PCM de 16 bits, estéreo, 48000 Hz.")
        print('Se activará tu audio como custom_sound.wav. El original se conserva.')
        install_files(sd,'audio',backup,custom_wav=converted)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke-test',action='store_true',help=argparse.SUPPRESS)
    args=parser.parse_args()
    if args.smoke_test:
        from tools.portable_runtime import platform_key
        from tools.audio_convert import validate_wav
        from tools.partition_info import partition_info
        print('Asistente disponible:',platform_key(),(ROOT/'VERSION').read_text().strip());return
    print('Nintendo Switch · BT & Beats — Sonido y controles\n1. Sonido y conversor de audio\n2. Imagen de Hekate\n3. Preparar instalación Bluetooth nueva\n4. Actualizar/reparar el único sincronizador Bluetooth (con respaldo)')
    choice=input('Elige 1, 2, 3 o 4: ').strip()
    if choice not in ('1','2','3','4'):raise ValueError('Opción no válida')
    value=input('Raíz de la microSD (E:/, /Volumes/SWITCH SD o /media/usuario/SWITCH SD): ').strip().strip('"')
    if not value:raise ValueError('Debes seleccionar la microSD')
    if re.fullmatch('[A-Za-z]:',value):value+='/'
    sd=Path(value).expanduser().resolve()
    if not (sd/'bootloader/hekate_ipl.ini').is_file() or not (sd/'atmosphere/package3').is_file():
        raise ValueError('Selecciona una microSD ya configurada con Hekate y Atmosphère')
    backup=Path.home()/'Documents/SwitchBackups'/('jherald-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    if choice=='1':audio_flow(sd,backup);return
    if choice=='2':install_files(sd,'logo',backup,entry=choose_entry(sd,'emuMMC'));return
    original=choose_entry(sd,'sysMMC');hack=choose_entry(sd,'emuMMC')
    profile=input('Firmware de sysMMC (perfil admitido: 23.0.1): ').strip()
    controllers=input('Mandos: 1 Pro/Joy-Con (conserva el perfil anterior) / 2 MissionControl experimental [1]: ').strip() or '1'
    if controllers not in ('1','2'):raise ValueError('Perfil de mandos no válido')
    controller_profile='missioncontrol' if controllers=='2' else 'nintendo'
    if controllers=='2':print('Candidatos limitados a modelos Sony/Xbox identificados; requiere MissionControl y emparejamiento válido en el origen. Sin garantía de prueba física.')
    if choice=='4':
        from tools.repair_bluetooth import repair_plan,apply_repair
        plan=repair_plan(sd,original,hack,profile,controller_profile)
        print('Se actualizará BT-Sync-Una-Vez.bin. Las versiones anteriores se respaldan en PC; registros conservados:',plan['counts'])
        if input('Escribe INSTALAR para actualizarlo con respaldo privado: ').strip()=='INSTALAR':apply_repair(plan,backup)
        else:print('Cancelado. No se escribió en la SD.')
        return
    print('La base Bluetooth externa debe estar activada y creada al arrancar emuMMC una vez.')
    print('Preparación local automática. El compilador se descarga solo si falta (unos 300 MiB).')
    from tools.bluetooth_setup import prepare_bluetooth,BT
    prepare_bluetooth(sd,original,hack,profile,controller_profile)
    print('Se añadirán cuatro archivos. El menú y la base actual se conservan. No se sincroniza durante la instalación.')
    if input('Escribe INSTALAR para guardar el respaldo privado e instalar el payload: ').strip()=='INSTALAR':
        run(['deploy_portable.py','--sd',sd,'--apply','--backup-dir',backup],BT)
        print('Después: Hekate > Payloads > BT-Sync-Una-Vez.bin. Comprueba OK y la reconexión en emuMMC.')
    else:print('Cancelado. No se escribió en la SD.')

if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('\nCancelado. Revisa el último paso mostrado antes de volver a ejecutar.');sys.exit(130)
    except (ValueError,OSError,subprocess.CalledProcessError) as exc:
        print('No se pudo completar:',exc,file=sys.stderr);sys.exit(1)
