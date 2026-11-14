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
sys.path.insert(0,str(BT))
from deploy_portable import plan_for
import prepare
import json
prep=json.loads((BT/'output/preparation.json').read_text())
sd=Path(prep['sd'])
for loader,controllers in [('6.5.4','nintendo'),('6.5.5','nintendo'),('6.5.5','missioncontrol')]:
    (sd/'bootloader/update.bin').write_bytes((BT/f'test-assets/hekate-{loader}.bin').read_bytes())
    if controllers=='missioncontrol':
        mc=sd/'atmosphere/contents/010000000000bd00';(mc/'flags').mkdir(parents=True,exist_ok=True)
        (mc/'exefs.nsp').write_bytes(b'synthetic presence fixture, not a sysmodule');(mc/'flags/boot2.flag').write_bytes(b'')
    prepare.prepare(sd,'Stock OFW','CFW emuMMC',4096,100000,'23.0.1',controllers)
    run(['build_payload.py'],BT);run(['audit_auto.py'],BT)
    plan,additions=plan_for(sd,check_partition=False)
    assert len(additions)==4
    print('Synthetic profile verified:',loader,controllers)
print('Portable conversion/build/audit/installation-plan verified; synthetic target only')
