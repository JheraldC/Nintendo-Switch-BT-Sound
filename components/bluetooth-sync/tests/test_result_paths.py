"""Exercise the actual result-control functions with synthetic host I/O."""
from pathlib import Path
import hashlib,json,os,re,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'output';OUT.mkdir(exist_ok=True)
main=(ROOT/'src/main.c').read_text()
actual=main[main.index('static void show_progress(unsigned step,const char *label){'):main.index('void ipl_main(')]
prefix=r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auto_sync.h"
#include "auto_platform.h"
#include "auto_binding.h"
#include "bt_save_reader.h"
typedef unsigned u32;
typedef struct {BsrReader *reader;} BtoTegraReader;
typedef struct {unsigned unused;} BtdBtSummary;
enum {NO_CHANGES,UPDATED,BAD_GUARDS,BAD_RECOVERY,BAD_DB_READ,BAD_STATE_READ,
      BAD_DB_DECODE,BAD_STATE_DECODE,BAD_OPEN,BAD_COLLECT,BAD_MERGE,
      BAD_DB_ENCODE,BAD_STATE_ENCODE,TX_RECOVERED,TX_NOT_RECOVERED,SCENARIOS};
static int scenario,alloc_step,fail_alloc,recover_calls;
static unsigned checks,wait_ms,clear_count,colour,progress_stage,bar_right;
static bool screen_ready=true;
static char label[64];
static struct {bool mute;} gfx_con;
static void gfx_clear_grey(unsigned shade){assert(shade==0x1b);clear_count++;}
static void gfx_con_setcol(unsigned fg,int fill,unsigned bg){assert(fill==1&&bg==0xFF1B1B1B);colour=fg;}
static void gfx_con_setpos(unsigned x,unsigned y){assert(x<=1279&&(y==312||y==350||y==352));}
static void gfx_box(int x0,int y0,int x1,int y1,unsigned c){
 assert(x0>=0&&x0<=x1&&x1<=1279&&y0>=0&&y0<=y1&&y1<=719);
 if(c==0xFF55BBD0){assert(x0==402&&y0==392&&x1<=877&&y1==407);bar_right=(unsigned)x1;}
}
static void gfx_printf(const char *fmt,...){if(gfx_con.mute)return;va_list ap;va_start(ap,fmt);vsnprintf(label,sizeof(label),fmt,ap);va_end(ap);}
static void msleep(unsigned ms){if(ms==500)assert(progress_stage==7&&bar_right==877);wait_ms=ms;}
void bth_wipe(void *p,size_t n){memset(p,0,n);}
static void disconnectMMC(void){}
static void se_aes_key_clear(unsigned slot){assert(slot==4||slot==5||slot==10);}
static void bto_tegra_close(BtoTegraReader *r){r->reader=NULL;}
static BsrResult bto_tegra_open_from_sd(BtoTegraReader *r,BsrError *e){(void)r;(void)e;return scenario==BAD_OPEN?BSR_AUTH:BSR_OK;}
static BsrResult btd_bt_collect(BsrReader *r,BtdBtSummary *s,BthRecords *out){(void)r;(void)s;(void)out;return scenario==BAD_COLLECT?BSR_IO:BSR_OK;}
static int read_stub(void *ctx,enum BtaPath id,uint8_t *out,size_t cap,size_t *n){
 (void)ctx;(void)cap;if((id==BTA_DB&&scenario==BAD_DB_READ)||(id==BTA_STATE&&scenario==BAD_STATE_READ))return -1;
 *n=id==BTA_DB?8:96;memset(out,0,*n);return 1;
}
const BtaIo *bta_platform_io(void){static const BtaIo io={.read=read_stub};return &io;}
int bta_platform_guards(void){return scenario!=BAD_GUARDS;}
bool bta_recover(const BtaIo *io,const uint8_t cfg[32]){(void)io;(void)cfg;recover_calls++;return scenario!=BAD_RECOVERY&&!(scenario==TX_NOT_RECOVERED&&recover_calls>1);}
bool bta_database_decode(const uint8_t *p,size_t n,BthRecords *out){(void)p;(void)n;(void)out;return scenario!=BAD_DB_DECODE;}
bool bta_state_decode(const uint8_t *p,size_t n,const uint8_t cfg[32],BtaState *out){(void)p;(void)n;(void)cfg;(void)out;return scenario!=BAD_STATE_DECODE;}
bool bta_merge(const BthRecords *d,const BthRecords *s,const BtaState *before,BthRecords *out,BtaState *after,BtaStats *stats){
 (void)d;(void)s;(void)before;(void)out;(void)after;stats->database_changed=scenario!=NO_CHANGES;return scenario!=BAD_MERGE;
}
bool bta_database_encode(const BthRecords *r,uint8_t *out,size_t cap,size_t *n){(void)r;(void)out;(void)cap;*n=8;return scenario!=BAD_DB_ENCODE;}
bool bta_state_encode(const BtaState *s,const uint8_t cfg[32],uint8_t *out,size_t cap,size_t *n){(void)s;(void)cfg;(void)out;(void)cap;*n=96;return scenario!=BAD_STATE_ENCODE;}
bool bta_transaction(const BtaIo *io,const uint8_t cfg[32],const uint8_t *a,size_t an,const uint8_t *b,size_t bn,const uint8_t *c,size_t cn,const uint8_t *d,size_t dn){
 (void)io;(void)cfg;(void)a;(void)an;(void)b;(void)bn;(void)c;(void)cn;(void)d;(void)dn;return scenario!=TX_RECOVERED&&scenario!=TX_NOT_RECOVERED;
}
static void *test_malloc(size_t n){return ++alloc_step==fail_alloc?NULL:malloc(n);}
static void *test_calloc(size_t n,size_t size){return ++alloc_step==fail_alloc?NULL:calloc(n,size);}
#define malloc test_malloc
#define calloc test_calloc
'''
suffix=r'''
#undef malloc
#undef calloc
int main(void){
 for(scenario=0;scenario<SCENARIOS;scenario++){
  alloc_step=fail_alloc=recover_calls=0;gfx_con.mute=true;progress_stage=bar_right=wait_ms=0;
  assert(synchronize()==(scenario==NO_CHANGES||scenario==UPDATED));checks++;
  assert((progress_stage==7)==(scenario==NO_CHANGES||scenario==UPDATED)&&wait_ms==((scenario==NO_CHANGES||scenario==UPDATED)?500u:0u));checks++;
 }
 scenario=UPDATED;
 for(fail_alloc=1;fail_alloc<=9;fail_alloc++){
  alloc_step=recover_calls=0;gfx_con.mute=true;progress_stage=wait_ms=0;assert(synchronize()==0&&progress_stage<7&&wait_ms==0);checks++;
 }
 gfx_con.mute=true;progress_stage=bar_right=0;show_progress(0,"Preparando...");
 assert(gfx_con.mute&&progress_stage==0&&!strcmp(label,"Preparando..."));checks++;
 show_progress(3,"Leyendo sysMMC...");assert(progress_stage==3&&bar_right==401+476*3/7);checks++;
 unsigned saved_right=bar_right;show_progress(2,"No retroceder");assert(progress_stage==3&&bar_right==saved_right);checks++;
 show_progress(100,"Finalizando...");assert(progress_stage==7&&bar_right==877);checks++;
 screen_ready=false;progress_stage=0;show_progress(1,"No dibujar");assert(progress_stage==0);screen_ready=true;checks++;
 gfx_con.mute=true;clear_count=wait_ms=colour=0;show_result(1);
 assert(!gfx_con.mute&&clear_count==1&&wait_ms==3000&&colour==0xFF00FF00&&!strcmp(label,"OK"));checks++;
 gfx_con.mute=true;clear_count=wait_ms=colour=0;show_result(0);
 assert(!gfx_con.mute&&clear_count==1&&wait_ms==3000&&colour==0xFFFF0000&&!strcmp(label,"FALLO"));checks++;
 printf("Result paths: %u checks passed; real stage progress stays monotonic; failures cannot complete the bar; result colours and 3s delay preserved\n",checks);
 return 0;
}
'''
source=OUT/'test_result_paths.c';binary=OUT/'test_result_paths';source.write_text(prefix+actual+suffix)
def host_path(path):
    text=path.as_posix()
    return '/mnt/'+text[0].lower()+text[2:] if os.name=='nt' else text
launcher=['wsl.exe','-e'] if os.name=='nt' else []
subprocess.run(launcher+['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-const-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+host_path(ROOT/'include'),'-I'+host_path(ROOT/'module/vendor/core/include'),host_path(source),'-o',host_path(binary)],check=True)
result=subprocess.run(launcher+[host_path(binary)],check=True,capture_output=True,text=True)
print(result.stdout,end='')
count=int(re.search(r'Result paths: (\d+) checks',result.stdout)[1])
(OUT/'result-ui-tests.json').write_text(json.dumps({'passed':True,'checks':count,'asan_ubsan':True,'main_source_sha256':hashlib.sha256((ROOT/'src/main.c').read_bytes()).hexdigest(),'test_source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'success_colour':'0xFF00FF00','failure_colour':'0xFFFF0000','display_ms':3000,'hardware_tested':False},indent=2))
