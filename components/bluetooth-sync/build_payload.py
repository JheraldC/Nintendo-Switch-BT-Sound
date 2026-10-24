from pathlib import Path
import hashlib,json,subprocess,sys
from toolchain import tool
ROOT=Path(__file__).resolve().parent;TE=ROOT/'vendor/tegraexplorer';MOD=ROOT/'module';OUT=ROOT/'output'
OUT.mkdir(exist_ok=True)
freeze=json.loads((ROOT/'core-freeze.json').read_text(encoding='utf-8'))
for item in freeze:
    actual=MOD/'vendor/core'/item['path']
    assert hashlib.sha256(actual.read_bytes()).hexdigest()==item['sha256'],'frozen core changed: '+item['path']
CC=tool('gcc')
OBJCOPY=tool('objcopy');NM=tool('nm');SIZE=tool('size')
arch=['-march=armv4t','-mtune=arm7tdmi','-mthumb','-mthumb-interwork']
flags=arch+['-Os','-g','-ffunction-sections','-fdata-sections','-fomit-frame-pointer','-std=gnu11','-Wall','-Wno-missing-braces','-Wno-incompatible-pointer-types','-Wno-implicit-function-declaration','-Wno-int-conversion','-DIPL_LOAD_ADDR=0x40008000']
flags+=['-DBTO_DIAGNOSTIC','-DFFCFG_INC="'+(TE/'source/libs/fatfs/ffconf.h').as_posix()+'"','-DGFX_INC="'+(TE/'source/gfx/gfx.h').as_posix()+'"']
includes=[ROOT/'include',TE,TE/'bdk',MOD/'include',MOD/'tegra',MOD/'vendor/core/include',MOD/'vendor/core/src']
flags+=['-I'+str(p) for p in includes]
sources=[ROOT/'src/main.c',ROOT/'src/native_storage.c',ROOT/'src/nand_policy.c',ROOT/'src/diag_report.c',ROOT/'src/header_probe.c',MOD/'src/bt_only.c',MOD/'src/bt_keys.c',MOD/'tegra/tegra_reader.c',MOD/'tegra/tegra_keys.c']
sources+=sorted(p for p in (MOD/'vendor/core/src').glob('*.c') if p.name not in ('integrity.c','filesystem.c'))
sources+=[ROOT/'src/ivfc_salt.c',ROOT/'src/ivfc_bridge.c',ROOT/'src/save_filesystem.c',ROOT/'src/bt_probe.c',ROOT/'src/transfer.c',ROOT/'src/auto_sync.c',ROOT/'src/auto_transaction.c',ROOT/'src/auto_platform.c',ROOT/'src/auto_boot.c',ROOT/'src/auto_boot_config.c']
sources+=[TE/'source/start.S',TE/'bdk/exception_handlers.S',TE/'source/gfx/gfx.c',TE/'source/storage/nx_sd.c',TE/'source/libs/fatfs/ffsystem.c']
for directory in ['soc','mem','power','display','rtc','thermal']:
    sources+=sorted(p for p in (TE/'bdk'/directory).glob('*.c') if p.name not in ['minerva.c','sdram_lp0.c','smmu.c','ccplex.c','kfuse.c'])
sources+=[TE/'bdk'/p for p in ['sec/se.c','storage/sdmmc.c','storage/sdmmc_driver.c','utils/btn.c','utils/util.c','libs/fatfs/ff.c','libs/fatfs/ffunicode.c']]
objects=[];warnings=[]
for source in sources:
    rel=source.relative_to(ROOT).as_posix();obj=OUT/'obj'/(rel.replace('/','_')+'.o');obj.parent.mkdir(exist_ok=True)
    strict=['-Wextra','-Werror','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-Werror=int-conversion'] if TE not in source.parents else []
    process=subprocess.run([str(CC),*flags,*strict,'-c',str(source),'-o',str(obj)],capture_output=True,text=True)
    if process.stderr:warnings.append(process.stderr)
    if process.returncode:print(process.stdout+process.stderr);sys.exit(process.returncode)
    objects.append(obj)
(OUT/'compiler-warnings.log').write_text('\n'.join(warnings),encoding='utf-8')
elf=OUT/'bt-autosync-v1.elf';mapfile=OUT/'bt-autosync-v1.map'
command=[str(CC),*arch,'-nostartfiles','-Wl,--nmagic,--gc-sections,--wrap=f_open,--wrap=f_write,--wrap=f_unlink,--wrap=f_rename,--wrap=sdmmc_storage_write','-Wl,-Map='+str(mapfile),'-Wl,--defsym=IPL_LOAD_ADDR=0x40008000','-T',str(TE/'source/link.ld'),*[str(p) for p in objects],'-lgcc','-o',str(elf)]
process=subprocess.run(command,capture_output=True,text=True)
if process.returncode:print(process.stdout+process.stderr);sys.exit(process.returncode)
binary=OUT/'bt-autosync-v1.bin'
symbols=subprocess.check_output([str(NM),'-n',str(elf)],text=True);(OUT/'symbols.txt').write_text(symbols,encoding='utf-8')
size=subprocess.check_output([str(SIZE),str(elf)],text=True);print(size.strip())
for forbidden in ['f_mkfs','save_commit','GetKeysFromFile','DumpKeys','RunScript','EnterMainMenu','launch_payload','fuse_set_sbk','fuse_read_sbk','se_get_aes_keys','se_aes_key_get','emummc_storage_write','nx_emmc_bis_write','sdmmc_storage_execute_vendor_cmd','sdmmc_storage_get_vendor_data']:
    assert not any(line.split()[-1:]==[forbidden] for line in symbols.splitlines()),'forbidden symbol linked: '+forbidden
limits={line.split()[-1]:int(line.split()[0],16) for line in symbols.splitlines() if len(line.split())==3 and line.split()[-1] in ['__ipl_start','__ipl_end','__bss_end']}
assert limits['__bss_end']<=0x40038000,'IRAM image overlaps relocation area'
subprocess.run([str(OBJCOPY),'-S','-O','binary','--gap-fill=0','--pad-to='+hex(limits['__ipl_end']),str(elf),str(binary)],check=True)
assert binary.stat().st_size<=126296,'payload exceeds uncompressed upstream size budget'
record={'binary':binary.name,'bytes':binary.stat().st_size,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'objects':len(objects),'limits':limits,'frozen_core_files_verified':len(freeze),'source_files':[p.relative_to(ROOT).as_posix() for p in sources],'hardware_tested':False,'purpose':'manual Original-to-Hack sync; readonly NAND; scoped SD transaction'}
record['binding_sha256']=hashlib.sha256((ROOT/'include/auto_binding.h').read_bytes()).hexdigest()
record['compiler_version']=subprocess.check_output([str(CC),'--version'],text=True).splitlines()[0]
inputs=[p for folder in ('src','include','module','vendor') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.S') and 'build' not in p.relative_to(ROOT).parts]
record['compiled_sources']={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}
(OUT/'build-evidence.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
print('PAYLOAD BUILT:',binary.name,binary.stat().st_size,'bytes; no forbidden symbols; IRAM bounds passed')
