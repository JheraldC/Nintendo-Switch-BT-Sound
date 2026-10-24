/* SPDX-License-Identifier: GPL-2.0-only */
#include "diag_native.h"
#include "source/config.h"
#include "source/gfx/gfx.h"
#include <display/di.h>
#include <mem/heap.h>
#include <memory_map.h>
#include <soc/hw_init.h>
#include <soc/t210.h>
#include <soc/bpmp.h>
#include <storage/nx_sd.h>
#include <utils/btn.h>
#include <utils/util.h>
#include "tegra_reader.h"
#include "bt_only.h"
#include "bt_probe.h"
#include "auto_sync.h"
#include "auto_platform.h"
#include "auto_binding.h"
#include <stdlib.h>
#include <string.h>
hekate_config h_cfg;
boot_cfg_t __attribute__((section("._boot_cfg"))) b_cfg;
volatile nyx_storage_t *nyx_str=(nyx_storage_t *)NYX_STORAGE_ADDR;
extern void pivot_stack(u32);
static bool screen_ready;
static int first_error;
static unsigned progress_stage;
static void show_progress(unsigned step,const char *label);
void btd_status(const char *stage,int result){
    if(result>0&&!first_error)first_error=result;
    if(screen_ready&&result==0&&!strcmp(stage,"NAND_INIT"))show_progress(3,"Leyendo sysMMC...");
    if(screen_ready&&result==0&&!strcmp(stage,"CORE_READER"))show_progress(4,"Leyendo mandos...");
}
void btd_number(const char *label,u32 value){(void)label;(void)value;}
void btd_skip(const char *label){(void)label;}
static void show_progress(unsigned step,const char *label){
    if(!screen_ready||step<progress_stage)return;
    if(step>7)step=7;
    progress_stage=step;
    bool muted=gfx_con.mute;gfx_con.mute=false;
    gfx_con_setcol(0xFFCCCCCC,1,0xFF1B1B1B);
    gfx_con_setpos(552,312);gfx_printf("Cargando...");
    gfx_box(0,344,1279,367,0xFF1B1B1B);
    gfx_con_setpos(640-(unsigned)strlen(label)*8,350);gfx_printf("%s",label);
    gfx_box(400,390,879,409,0xFF555555);
    gfx_box(402,392,877,407,0xFF2F2F2F);
    if(step)gfx_box(402,392,401+(int)(476*step/7),407,0xFF55BBD0);
    gfx_con.mute=muted;
}
static void show_result(int success){
    gfx_con.mute=false;gfx_clear_grey(0x1b);
    gfx_con_setcol(success?0xFF00FF00:0xFFFF0000,1,0xFF1B1B1B);
    gfx_con_setpos(success?624:600,352);gfx_printf(success?"OK":"FALLO");
    msleep(3000);
}
static void release(void *p,size_t n){if(p){bth_wipe(p,n);free(p);}}
static int synchronize(void){
    const BtaIo *io=bta_platform_io();
    show_progress(1,"Comprobando datos...");
    BthRecords *source=calloc(1,sizeof(*source)),*destination=calloc(1,sizeof(*destination)),*merged=calloc(1,sizeof(*merged));
    BtaState *before=calloc(1,sizeof(*before)),*after=calloc(1,sizeof(*after));
    uint8_t *old_db=malloc(BTA_DB_MAX),*new_db=malloc(BTA_DB_MAX),*old_state=malloc(BTA_STATE_MAX),*new_state=malloc(BTA_STATE_MAX);
    size_t old_db_size=0,new_db_size=0,old_state_size=0,new_state_size=0;int boot_hack=0,completed=0;
    BtoTegraReader context={0};BsrError error={0};BtdBtSummary summary={0};BtaStats stats={0};
    if(!source||!destination||!merged||!before||!after||!old_db||!new_db||!old_state||!new_state){gfx_printf("Memoria insuficiente. Se vuelve a Hekate.\n");goto done;}
    if(!bta_platform_guards()){gfx_printf("Configuracion cambiada. Sincronizacion suspendida.\nSe vuelve a Hekate sin modificar sysMMC.\n");goto done;}
    if(!bta_recover(io,bta_config_hash)){gfx_printf("No se pudo validar/recuperar la base SD.\nSe conserva la informacion para revisar.\n");goto done;}
    show_progress(2,"Leyendo datos actuales...");
    if(io->read(io->context,BTA_DB,old_db,BTA_DB_MAX,&old_db_size)!=1||!bta_database_decode(old_db,old_db_size,destination)
       ||io->read(io->context,BTA_STATE,old_state,BTA_STATE_MAX,&old_state_size)!=1||!bta_state_decode(old_state,old_state_size,bta_config_hash,before)){
        gfx_printf("Lista o historial no valido. Se vuelve a Hekate.\n");goto done;
    }
    boot_hack=1; /* An already validated last-known database is available. */
    show_progress(3,"Leyendo sysMMC...");
    BsrResult result=bto_tegra_open_from_sd(&context,&error);
    if(result==BSR_OK){show_progress(4,"Leyendo mandos...");result=btd_bt_collect(context.reader,&summary,source);}
    bto_tegra_close(&context);disconnectMMC();se_aes_key_clear(4);se_aes_key_clear(5);se_aes_key_clear(10);
    if(result!=BSR_OK){gfx_printf("Lectura no completada (%d).\nSe mantiene la lista de emuMMC: %d registros.\n",(u32)result,(u32)destination->count);goto done;}
    show_progress(5,"Comparando mandos...");
    if(!bta_merge(destination,source,before,merged,after,&stats)){
        gfx_printf("Sincronizacion aplazada: limite de mandos.\nSe mantiene la lista de emuMMC: %d registros.\n",(u32)destination->count);goto done;
    }
    if(!stats.database_changed&&!stats.history_changed){completed=1;goto done;}
    if(!bta_database_encode(merged,new_db,BTA_DB_MAX,&new_db_size)||!bta_state_encode(after,bta_config_hash,new_state,BTA_STATE_MAX,&new_state_size)){
        gfx_printf("No se pudo preparar la actualizacion. Lista anterior conservada.\n");goto done;
    }
    show_progress(6,"Guardando cambios...");
    if(!bta_transaction(io,bta_config_hash,old_db,old_db_size,old_state,old_state_size,new_db,new_db_size,new_state,new_state_size)){
        boot_hack=bta_recover(io,bta_config_hash)?1:0;
        gfx_printf(boot_hack?"Actualizacion incompleta recuperada.\nAbre este payload otra vez para reintentar.\n":"Actualizacion incompleta. Se vuelve a Hekate.\nNo se modifica la NAND sysMMC.\n");goto done;
    }
    completed=1;
 done:
    bto_tegra_close(&context);disconnectMMC();se_aes_key_clear(4);se_aes_key_clear(5);se_aes_key_clear(10);
    release(source,sizeof(*source));release(destination,sizeof(*destination));release(merged,sizeof(*merged));
    release(before,sizeof(*before));release(after,sizeof(*after));release(old_db,BTA_DB_MAX);release(new_db,BTA_DB_MAX);
    release(old_state,BTA_STATE_MAX);release(new_state,BTA_STATE_MAX);bth_wipe(&summary,sizeof(summary));bth_wipe(&stats,sizeof(stats));
    if(completed){show_progress(7,"Finalizando...");msleep(500);}
    return completed;
}
void ipl_main(void){
    hw_init();pivot_stack(IPL_STACK_TOP);heap_init(IPL_HEAP_START);
    h_cfg.t210b01=hw_get_chip_id()==GP_HIDREV_MAJOR_T210B01;h_cfg.emummc_force_disable=1;h_cfg.backlight=100;
    display_init();u32 *fb=display_init_framebuffer_pitch();gfx_init_ctxt(fb,720,1280,720);gfx_con_init();
    display_backlight_pwm_init();display_backlight_brightness(100,500);screen_ready=true;gfx_con.mute=true;gfx_clear_grey(0x1b);show_progress(0,"Preparando...");
    if(!sd_mount()){gfx_printf("No se pudo montar la SD.\n");goto failed;}
    if(!bta_prepare_boot()){gfx_printf("No se pudo validar la continuacion de Hekate.\n");goto failed;}
    if(btn_read()&BTN_VOL_DOWN){show_result(0);bta_continue_boot(0);goto failed;}
    int success=synchronize();show_result(success);
    bta_continue_boot(0);
 failed:
    show_result(0);power_set_state(POWER_OFF);while(1)bpmp_halt();
}
