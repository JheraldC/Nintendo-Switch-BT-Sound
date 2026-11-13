"""Comprueba descarga, conversión y construcción sintética sin una SD real."""
from pathlib import Path
import os,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.audio_convert import download_runtime,runtime_directory
from tools.portable_runtime import ensure_arm_toolchain
from tools.bluetooth_setup import verified_host_evidence,BT

def run(args,cwd=ROOT):subprocess.run([sys.executable,*map(str,args)],cwd=cwd,check=True)

os.environ['FFMPEG_BINARY']=download_runtime(runtime_directory())
run(['-m','unittest','discover','-s','tests','-v'])
run(['tests/test_portable.py'],BT)
run(['tests/test_portable.py','--prepare-synthetic'],BT)
verified_host_evidence()
ensure_arm_toolchain(force_portable=True)
run(['build_payload.py'],BT)
run(['audit_auto.py'],BT)
sys.path.insert(0,str(BT))
from deploy_portable import plan_for
import json
prep=json.loads((BT/'output/preparation.json').read_text())
plan,additions=plan_for(Path(prep['sd']),check_partition=False)
assert len(additions)==4
print('Portable conversion/build/audit/installation-plan verified; synthetic target only')
