/* SPDX-License-Identifier: GPL-2.0-only */
#include "tegra_reader.h"
#include "tegra_keys.h"
#include "bt_only.h"
#include <limits.h>
#include <string.h>
#ifdef BTO_DIAGNOSTIC
#include "header_probe.h"
extern void btd_status(const char *,int);
extern void btd_number(const char *,uint32_t);
extern void btd_skip(const char *);
#endif
/* Defined only by io_denials.c, which must be linked with the documented wraps. */
extern bool bto_tegra_io_denials_linked(void);
static const char settings_path[]="bis:/save/8000000000000050";
static bool get_size(void *p,uint64_t *size) {
    BtoTegraReader *c=p;if(!c||!c->file_open||!size)return false;
    *size=(uint64_t)f_size(&c->file);return true;
}
static bool read_exact(void *p,uint64_t offset,void *out,size_t count) {
    BtoTegraReader *c=p;uint64_t size;
    if(!get_size(c,&size)||(!out&&count)||offset>size||(uint64_t)count>size-offset
        ||(uint64_t)(FSIZE_t)offset!=offset)return false;
    if(f_lseek(&c->file,(FSIZE_t)offset)!=FR_OK)return false;
    uint8_t *bytes=out;size_t done=0;
    while(done<count) {
        size_t chunk=count-done;if(chunk>4096)chunk=4096;
        UINT actual=0;
        if(f_read(&c->file,bytes+done,(UINT)chunk,&actual)!=FR_OK||actual!=chunk) {
            bto_wipe(out,count);return false;
        }
        done+=chunk;
    }
    return true;
}
static bool authenticate(void *unused,const uint8_t *header,size_t size) {
    (void)unused;
    if(!header||size!=0x4000)return false;
    _Alignas(4) uint8_t computed[16]={0};
    bool ok=se_aes_cmac(10,computed,sizeof(computed),header+0x100,0x200)!=0;
    volatile uint8_t diff=0;
    for(size_t i=0;i<16;i++)diff=(uint8_t)(diff|(uint8_t)(computed[i]^header[i]));
    ok=ok&&diff==0;bto_wipe(computed,sizeof(computed));bto_wipe((void *)&diff,sizeof(diff));
    return ok;
}
#ifdef BTO_DIAGNOSTIC
static bool probe_cmac(void *unused,const uint8_t *header,size_t size,uint8_t out[16]) {
    (void)unused;
    return size==0x4000&&se_aes_cmac(10,out,16,header+0x100,0x200)!=0;
}
static void show_headers(const BtdHeaderSummary *s) {
    const char *read_name[]={"HDR_A_READ","HDR_B_READ"};
    const char *mac_name[]={"HDR_A_CMAC","HDR_B_CMAC"};
    const char *engine_name[]={"HDR_A_AES","HDR_B_AES"};
    const char *magic_name[]={"HDR_A_DISF","HDR_B_DISF"};
    const char *version_name[]={"HDR_A_VERSION","HDR_B_VERSION"};
    const char *format_name[]={"HDR_A_FORMAT","HDR_B_FORMAT"};
    const char *sha_name[]={"HDR_A_SHA","HDR_B_SHA"};
    btd_number("SAVE_BYTES",(uint32_t)s->container_size);
    for(unsigned i=0;i<2;i++) {
        const BtdHeaderCheck *h=&s->header[i];btd_status(read_name[i],h->read?0:BSR_IO);
        if(!h->read){btd_skip(engine_name[i]);btd_skip(mac_name[i]);btd_skip(magic_name[i]);btd_skip(version_name[i]);btd_skip(format_name[i]);btd_skip(sha_name[i]);continue;}
        btd_status(engine_name[i],h->cmac_engine?0:BSR_IO);
        btd_status(mac_name[i],h->cmac_match?0:BSR_AUTH);
        btd_status(magic_name[i],h->magic?0:BSR_FORMAT);
        if(h->magic)btd_number(version_name[i],h->version);else btd_skip(version_name[i]);
        if(h->magic)btd_status(format_name[i],h->version_ok?0:BSR_UNSUPPORTED);else btd_skip(format_name[i]);
        btd_status(sha_name[i],h->sha_match?0:BSR_INTEGRITY);
    }
    if(s->header[0].read&&s->header[1].read)btd_status("HDR_A_B_IDENTICAL",s->identical?0:BSR_AMBIGUOUS);
    else btd_skip("HDR_A_B_IDENTICAL");
    static const char *const part[BTD_LAYOUT_PARTS]={"FMT_DPFS","FMT_DATA_IVFC","FMT_JOURNAL","FMT_SAVE","FMT_MAIN_RMAP","FMT_META_RMAP","FMT_FAT_IVFC"};
    static const char *const ver[BTD_LAYOUT_PARTS]={"DPFS_VERSION","DATA_IVFC_VERSION","JOURNAL_VERSION","SAVE_VERSION","MAIN_RMAP_VERSION","META_RMAP_VERSION","FAT_IVFC_VERSION"};
    if(s->layout_checked) {
        for(unsigned i=0;i<BTD_LAYOUT_PARTS;i++) {
            if(!(s->layout_mask&(1u<<i))){btd_skip(part[i]);continue;}
            btd_status(part[i],s->layout_valid&(1u<<i)?0:BSR_UNSUPPORTED);
            if(s->layout_version[i])btd_number(ver[i],s->layout_version[i]);
        }
        if(s->layout_valid&(1u<<BTD_MAIN_RMAP)) {
            btd_number("MAIN_RMAP_ENTRIES",s->remap_geometry[0][0]);btd_number("MAIN_RMAP_SEGMENTS",s->remap_geometry[0][1]);btd_number("MAIN_RMAP_BITS",s->remap_geometry[0][2]);
        }
        if(s->layout_valid&(1u<<BTD_META_RMAP)) {
            btd_number("META_RMAP_ENTRIES",s->remap_geometry[1][0]);btd_number("META_RMAP_SEGMENTS",s->remap_geometry[1][1]);btd_number("META_RMAP_BITS",s->remap_geometry[1][2]);
        }
        if(s->layout_valid&(1u<<BTD_DATA_IVFC))btd_number("DATA_IVFC_LEVELS",s->ivfc_levels[0]);
        if(s->layout_valid&(1u<<BTD_FAT_IVFC))btd_number("FAT_IVFC_LEVELS",s->ivfc_levels[1]);
    } else btd_skip("LAYOUT_DETAILS");
}
#endif
void bto_tegra_close(BtoTegraReader *c) {
    if(!c)return;
    bsr_close(&c->reader);
    if(c->file_open)f_close(&c->file);
    if(c->storage_connected)disconnectMMC();
    if(c->owns_bis_slots){se_aes_key_clear(4);se_aes_key_clear(5);}
    bto_wipe(c,sizeof(*c));se_aes_key_clear(10);
}
BsrResult bto_tegra_open(BtoTegraReader *c,const uint8_t key[16],BsrError *error) {
    if(!c||!key)return BSR_ARGUMENT;
    bto_wipe(c,sizeof(*c));
    uint8_t any=0;for(size_t i=0;i<16;i++)any|=key[i];
    if(!any||!bto_tegra_io_denials_linked())return BSR_AUTH;
    /* BIS SYSTEM slots must already be initialized by the caller. */
    if(connectMMC(MMC_CONN_EMMC)!=0)return BSR_IO;
    c->storage_connected=true;
    if(mountMMCPart("SYSTEM").err!=0) {
        bto_tegra_close(c);return BSR_IO;
    }
    FRESULT opened=f_open(&c->file,settings_path,FA_READ);
#ifdef BTO_DIAGNOSTIC
    btd_status("SAVE_OPEN",(int)opened);
#endif
    if(opened!=FR_OK){bto_tegra_close(c);return BSR_IO;}
    c->file_open=true;
    se_aes_key_set(10,key,16);
    BsrStorage storage={c,read_exact,get_size};
#ifdef BTO_DIAGNOSTIC
    BtdHeaderSummary summary={0};
    BsrResult preflight=btd_probe_headers(&storage,probe_cmac,NULL,&summary);
    show_headers(&summary);btd_status("HEADERS",(int)preflight);
    bto_wipe(&summary,sizeof(summary));
    if(preflight!=BSR_OK) {
        if(error)*error=(BsrError){preflight,"header preflight"};
        btd_skip("CORE_READER");bto_tegra_close(c);return preflight;
    }
    btd_status("CORE_READER",-1);
#endif
    BsrResult result=bsr_open(&storage,authenticate,NULL,&c->reader,error);
#ifdef BTO_DIAGNOSTIC
    if(result!=BSR_OK&&error&&error->stage)btd_status(error->stage,(int)result);
    btd_status("CORE_READER",(int)result);
#endif
    se_aes_key_clear(10);
    if(result!=BSR_OK)bto_tegra_close(c);
    return result;
}
BsrResult bto_tegra_open_from_sd(BtoTegraReader *c,BsrError *error) {
    if(!c)return BSR_ARGUMENT;
    bto_wipe(c,sizeof(*c));BtoKeys keys={0};
    BtoKeyResult loaded=bto_tegra_keys_load(&keys);
    if(loaded!=BTO_KEYS_OK) {
        bto_wipe(&keys,sizeof(keys));return loaded==BTO_KEYS_IO?BSR_IO:BSR_AUTH;
    }
    se_aes_key_set(4,keys.bis_system,16);se_aes_key_set(5,keys.bis_system+16,16);
    BsrResult result=bto_tegra_open(c,keys.save_mac,error);
    bto_wipe(&keys,sizeof(keys));
    if(result==BSR_OK)c->owns_bis_slots=true;
    else {se_aes_key_clear(4);se_aes_key_clear(5);}
    return result;
}
