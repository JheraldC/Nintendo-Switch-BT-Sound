"""Synthetic media, real frozen SHA, fake AES/CMAC; no console or keys needed."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'output';OUT.mkdir(exist_ok=True)
def run(directory,targets):
    if os.name=='nt':
        p=directory.as_posix();p='/mnt/'+p[0].lower()+p[2:]
        cmd=['wsl.exe','-e','make','-C',p,*targets]
    else:cmd=['make','-C',str(directory),*targets]
    result=subprocess.run(cmd,capture_output=True,text=True)
    print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
    if result.returncode:raise SystemExit(result.returncode)
    return result.stdout+result.stderr
log=run(ROOT,['test','auto-test','auto-platform-test','auto-boot-test','fatfs-map-test'])+run(ROOT/'module',['keys-test','sanitizers','adapter-test'])
(OUT/'host-tests.log').write_text(log,encoding='utf-8')
patterns={'fatfs_mapping':r'Actual FatFs mapping: (\d+) checks','auto_sync':r'Automatic sync: (\d+) checks','auto_platform':r'Actual SD platform: (\d+) checks','auto_boot':r'Hekate continuation: (\d+) checks','bluetooth_probe':r'BT probe: (\d+) checks','gpt':r'GPT v2: (\d+) checks','native':r'Native storage: (\d+) checks','report':r'Report: (\d+) checks',
          'headers':r'Header probe: (\d+) checks','diagnostic_adapter':r'tegra adapter: (\d+) checks',
          'console_geometry':r'Console geometry: (\d+) checks','filesystem_geometry':r'FS geometry: (\d+) checks','ivfc_hmac':r'IVFC HMAC: (\d+) checks','full_reader':r'PASS: (\d+) cases; ASan/UBSan'}
counts={}
for name,pattern in patterns.items():
    match=re.search(pattern,log);assert match,name
    counts[name]=int(match[1])
files=[p for p in ROOT.rglob('*') if p.is_file() and 'output' not in p.relative_to(ROOT).parts and
       'build' not in p.relative_to(ROOT).parts and (p.suffix in ('.c','.h') or p.name=='Makefile')]
record={'passed':True,'asan_ubsan':True,'counts':counts,'synthetic_media':True,'aes_and_cmac':'mocks',
        'sha256':'real frozen implementation','hardware_execution':False,
        'tested_sources':{p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}}
(OUT/'test-results.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
print('HOST TEST EVIDENCE SAVED:',counts)
