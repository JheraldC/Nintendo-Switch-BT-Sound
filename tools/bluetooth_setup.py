"""Compila y audita localmente el payload; no envía datos de la SD."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys
from tools.portable_runtime import ROOT,ensure_arm_toolchain

BT=ROOT/'components/bluetooth-sync'

def verified_host_evidence():
    evidence=BT/'evidence/host-tests.json'
    if not evidence.is_file():raise ValueError('Falta la evidencia de pruebas del núcleo; usa una versión publicada completa')
    data=json.loads(evidence.read_text(encoding='utf-8'))
    if not data.get('passed') or not data.get('asan_ubsan'):raise ValueError('La evidencia del núcleo no es válida')
    if data.get('binding_validation')!='generated locally; checked against compiled build':raise ValueError('Versión de evidencia no admitida')
    for name,expected in data['tested_sources'].items():
        if hashlib.sha256((BT/name).read_bytes()).hexdigest()!=expected:raise ValueError('Las fuentes cambiaron respecto a las pruebas: '+name)
    out=BT/'output';out.mkdir(exist_ok=True)
    shutil.copyfile(evidence,out/'test-results.json')
    ui=json.loads((BT/'evidence/result-ui-tests.json').read_text(encoding='utf-8'))
    if not ui.get('passed') or hashlib.sha256((BT/'src/main.c').read_bytes()).hexdigest()!=ui['main_source_sha256']:
        raise ValueError('La interfaz Bluetooth cambió respecto a sus pruebas')
    if hashlib.sha256((BT/'tests/test_result_paths.py').read_bytes()).hexdigest()!=ui['test_source_sha256']:
        raise ValueError('Las pruebas de interfaz cambiaron respecto a la evidencia')
    shutil.copyfile(BT/'evidence/result-ui-tests.json',out/'result-ui-tests.json')

def prepare_bluetooth(sd,original,hack,profile,controller_profile='nintendo'):
    # Preflight first: an unsupported SD must not trigger a large toolchain download.
    subprocess.run([sys.executable,'prepare.py','--sd',str(sd),'--sysmmc',original,'--emummc',hack,'--firmware-profile',profile,'--controller-profile',controller_profile],cwd=BT,check=True)
    print('Verificando pruebas publicadas del núcleo y preparando el compilador ARM...')
    verified_host_evidence()
    ensure_arm_toolchain()
    for script in ('build_payload.py','audit_auto.py'):
        subprocess.run([sys.executable,script],cwd=BT,check=True)
    subprocess.run([sys.executable,'deploy_portable.py','--sd',str(sd)],cwd=BT,check=True)
