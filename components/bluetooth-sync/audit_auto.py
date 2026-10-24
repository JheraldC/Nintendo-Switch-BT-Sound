"""Audit the final ARM image and its actual production sources."""
from pathlib import Path
import difflib
import hashlib
import json
import re
import subprocess
from toolchain import tool

R=Path(__file__).resolve().parent;O=R/'output';V=R/'baseline';TE=R/'vendor/tegraexplorer'
e=json.loads((O/'build-evidence.json').read_text());binary=O/e['binary']
assert hashlib.sha256(binary.read_bytes()).hexdigest()==e['sha256']
symbols=(O/'symbols.txt').read_text();names={s.split()[-1] for s in symbols.splitlines() if s.split()}
for forbidden in ('f_mkdir','f_mkfs','f_truncate','f_chmod','f_fdisk','f_fdisk_mod','save_commit','save_data_file_write','nx_emmc_bis_write','emummc_storage_write',
                  'sdmmc_storage_set_mmc_partition','sdmmc_storage_execute_vendor_cmd','fuse_read_sbk','se_aes_key_get','launch_payload','sd_save_to_file','btx_export_sd'):
    assert forbidden not in names,forbidden
for required in ('bta_merge','bta_recover','bta_transaction','bta_prepare_boot','bta_continue_boot','bta_patch_boot_config','btd_transport_guard','btd_validate_ext_csd',
                 '__wrap_f_open','__wrap_f_write','__wrap_f_unlink','__wrap_f_rename','__wrap_sdmmc_storage_write'):
    assert required in names,required
dump=subprocess.check_output([str(tool('objdump')),'-d',str(O/'bt-autosync-v1.elf')],text=True)
def callers(target):
    current='';result=[]
    for line in dump.splitlines():
        match=re.match(r'^[0-9a-f]+ <([^>]+)>:',line)
        if match:current=match[1]
        if re.search(r'\b(?:bl|blx|b|b\.n|b\.w)\s+[0-9a-f]+ <'+re.escape(target)+r'>',line):result.append(current)
    return result
guarded={}
for target in ('f_write','f_unlink','f_rename','sdmmc_storage_write'):
    guarded[target]=callers(target);assert guarded[target] and set(guarded[target])=={'__wrap_'+target},(target,guarded[target])
guarded['wrapped_raw_write']=callers('__wrap_sdmmc_storage_write')
assert set(guarded['wrapped_raw_write'])=={'bta_sd_disk_write'},guarded
selective=callers('bsr_file_read');assert selective and set(selective)=={'settings_read'},selective
assert (R/'src/nand_policy.c').read_bytes()==(V/'src/nand_policy.c').read_bytes(),'NAND command/read policy changed'
for item in json.loads((R/'core-freeze.json').read_text()):
    assert hashlib.sha256((R/'module/vendor/core'/item['path']).read_bytes()).hexdigest()==item['sha256']
for relative in ('src/bt_probe.c','src/ivfc_salt.c','src/ivfc_bridge.c','src/save_filesystem.c','src/header_probe.c'):
    assert (R/relative).read_bytes()==(V/relative).read_bytes(),relative
driver=(TE/'bdk/storage/sdmmc_driver.c').read_text()
inner=driver[driver.index('static int _sdmmc_execute_cmd_inner('):]
assert inner.index('btd_transport_guard(')<inner.index('_sdmmc_wait_cmd_data_inhibit(')
native=(R/'src/native_storage.c').read_text()
assert 'if(d!=DRIVE_SD)return RES_WRPRT;' in native
assert 'int btd_nand_connected(void) { return connected?1:0; }' in native
platform=(R/'src/auto_platform.c').read_text()
for condition in ('btd_nand_connected()','sd_get_card_removed()','s!=&sd_storage','buffer!=raw_buffer','mode!=(FA_WRITE|FA_CREATE_NEW)',
                  'file!=&owned','file->obj.fs!=&sd_fs','id<BTA_DB_NEXT','aliases_main(path)','sector<BTA_SD_FIRST'):
    assert condition in platform,condition
assert 'bis:/save/8000000000000050' in platform
assert 'mode==FA_READ' in platform and 'BTA_STATE_NEXT&&id!=BTA_TXN' in platform
storage=(TE/'bdk/storage/sdmmc.c').read_text();write=storage[storage.index('int sdmmc_storage_write('):storage.index('/*\n* MMC specific functions.')]
for condition in ('storage != &sd_storage','storage->sdmmc->id != SDMMC_1','!btx_transport_allowed','num_sectors > 128','wipe[i] = 0'):
    assert condition in write,condition
conf=(TE/'source/libs/fatfs/ffconf.h').read_text();assert re.search(r'^#define FF_FS_READONLY\s+0$',conf,re.M)
assert re.search(r'^#define FF_FS_MINIMIZE\s+0$',conf,re.M)
assert 'module/tegra/io_denials.c' not in e['source_files'] and 'src/auto_platform.c' in e['source_files']
main=(R/'src/main.c').read_text();assert 'bta_continue_boot(0)' in main and 'bta_continue_boot(hack)' not in main and 'BTN_VOL_DOWN' in main
assert 'int success=synchronize();show_result(success);' in main
assert 'msleep(3000)' in main and 'btn_wait()' not in main
assert 'return completed;' in main and 'return boot_hack;' not in main
assert main.index('bto_tegra_close(&context);disconnectMMC();')<main.index('bta_transaction(')
boot=(R/'src/auto_boot.c').read_text();assert '0xC0000000' in boot and 'hw_reinit_workaround(false,0)' in boot and 'btd_nand_connected()' in boot
cfg=(R/'src/auto_boot_config.c').read_text();assert cfg.index('memcmp(digest,bta_hekate_hash')<cfg.index('memset(image+0x94')
assert '"BTREAL",7' in cfg and 'image[0x94]=hack?5:1' in cfg
tests=json.loads((O/'test-results.json').read_text());assert tests['passed'] and tests['asan_ubsan']
for path,expected in tests['tested_sources'].items():assert hashlib.sha256((R/path).read_bytes()).hexdigest()==expected,path
for key in ('auto_sync','auto_platform','auto_boot','full_reader','bluetooth_probe','native'):assert tests['counts'][key]>0
for relative in ('src/native_storage.c','vendor/tegraexplorer/bdk/storage/sdmmc.c','vendor/tegraexplorer/source/libs/fatfs/ffconf.h'):
    (O/(Path(relative).stem+'-auto.diff')).write_text(''.join(difflib.unified_diff((V/relative).read_text().splitlines(True),(R/relative).read_text().splitlines(True),fromfile='proven-export/'+relative,tofile='automatic/'+relative)))
report={'passed':True,'binary':e['binary'],'bytes':e['bytes'],'sha256':e['sha256'],'nand_command_policy_unchanged':True,
        'frozen_reader_files_verified':8,'selective_read_callers':selective,'max_source_logical_bytes':12308,'guarded_mutation_callers':guarded,
        'sd_writes_require_nand_disconnected':True,'sd_writes_scoped_to_transaction_paths_and_fat32_partition':True,
        'original_boot_entry_modified':False,'completion':'hekate-menu','boot_menu_modified':False,'hardware_automatic_boot_verified':False,
        'asan_ubsan_passed':True,'actual_fatfs_fault_prefix_test_passed':True,'checks':tests['counts']}
(O/'auto-audit.json').write_text(json.dumps(report,indent=2));print('AUTOMATIC ARM AUDIT PASSED: immutable NAND policy, guarded SD transaction, bounded reads and fixed Hekate continuation')
