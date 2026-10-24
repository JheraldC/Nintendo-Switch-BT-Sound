#include "auto_sync.h"
#include "sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{checks++;assert(e);}while(0)
static unsigned checks;
static const uint8_t cfg[32]={42};
static void fill_record(uint8_t *r,unsigned id,const char *name,unsigned key){
    memset(r,0,512);r[0]=2;r[4]=(uint8_t)(id>>8);r[5]=(uint8_t)id;r[0x27]=0x25;r[0x29]=(uint8_t)key;r[0x39]=1;
    strcpy((char *)r+0xD6,name);
}
static void fixture(BthRecords *dest,BthRecords *src,BtaState *state){
    memset(src,0,sizeof(*src));src->count=7;
    for(unsigned i=0;i<7;i++)fill_record(src->record[i],i+1,i==0?"SYNTHETIC SPEAKER":i<3?"Pro Controller":i%2?"Joy-Con (R)":"Joy-Con (L)",i+10);
    CHECK(bta_state_from_source(src,state));memset(dest,0,sizeof(*dest));dest->count=5;
    unsigned slots[]={4,3,2,5,6};for(unsigned i=0;i<5;i++)memcpy(dest->record[i],src->record[slots[i]],512);
}
static void policy_tests(void){
    BthRecords dest,src,out;BtaState before,after;BtaStats stats;
    fixture(&dest,&src,&before);CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));
    CHECK(!stats.added&&!stats.updated&&!stats.database_changed&&!stats.history_changed);CHECK(!memcmp(&out,&dest,sizeof(out)));CHECK(out.count==5);
    uint8_t temp[512];memcpy(temp,src.record[0],512);memcpy(src.record[0],src.record[6],512);memcpy(src.record[6],temp,512);
    CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(!stats.history_changed&&!stats.database_changed);
    fixture(&dest,&src,&before);
    dest.record[2][0x29]^=1; /* Hack's own newer pairing must survive unchanged Original. */
    CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(!memcmp(&out,&dest,sizeof(out))&&!stats.database_changed);
    src.record[2][0x29]^=2;CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));
    CHECK(stats.updated==1&&stats.added==0&&stats.history_changed&&stats.database_changed);CHECK(!memcmp(out.record[2],src.record[2],512));
    CHECK(!memcmp(out.record[0],dest.record[0],1024));CHECK(!memcmp(out.record[3],dest.record[3],1024));
    fixture(&dest,&src,&before);fill_record(src.record[src.count++],99,"Joy-Con (R)",99);
    CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(out.count==6&&stats.added==1&&!stats.updated);CHECK(!memcmp(out.record[5],src.record[7],512));
    CHECK(bta_merge(&out,&src,&after,&dest,&before,&stats));CHECK(!stats.database_changed&&!stats.history_changed); /* repeat, no churn */
    fixture(&dest,&src,&before);fill_record(src.record[src.count++],99,"SYNTHETIC AUDIO",99);
    CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(!stats.database_changed&&stats.history_changed&&out.count==5);
    fixture(&dest,&src,&before);src.count--;CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(!stats.database_changed&&stats.history_changed&&out.count==5);
    fixture(&dest,&src,&before);src.record[1][0x29]^=1; /* explicitly re-paired historical Pro now becomes a NEW source event */
    CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(out.count==6&&stats.added==1);
    fixture(&dest,&src,&before);src.record[5][0x29]=0;CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));CHECK(!stats.database_changed);
    fixture(&dest,&src,&before);dest.count=24;for(unsigned i=5;i<24;i++)fill_record(dest.record[i],100+i,"Joy-Con (L)",42);
    fill_record(src.record[src.count++],99,"Joy-Con (R)",99);CHECK(!bta_merge(&dest,&src,&before,&out,&after,&stats));
    CHECK(!out.count&&!after.count&&!stats.added&&!stats.updated);
    CHECK(!bta_merge(&dest,&src,&before,&dest,&after,&stats));
    uint8_t bad[512];memset(bad,'X',sizeof(bad));CHECK(!bta_controller(bad));
}
static void codec_tests(void){
    BthRecords records,decoded;BtaState state,read;uint8_t blob[BTA_DB_MAX],history[BTA_STATE_MAX];size_t n=0,m=0;
    for(unsigned count=0;count<=24;count++){
        memset(&records,0,sizeof(records));records.count=count;for(unsigned i=0;i<count;i++)fill_record(records.record[i],i+1,"Joy-Con (L)",42);
        CHECK(bta_database_encode(&records,blob,sizeof(blob),&n));CHECK(bta_database_decode(blob,n,&decoded));CHECK(!memcmp(&records,&decoded,sizeof(records)));
        CHECK(bta_state_from_source(&records,&state));CHECK(bta_state_encode(&state,cfg,history,sizeof(history),&m));CHECK(bta_state_decode(history,m,cfg,&read));CHECK(!memcmp(&state,&read,sizeof(state)));
        for(size_t i=0;i<m;i++){history[i]^=1;CHECK(!bta_state_decode(history,m,cfg,&read));history[i]^=1;CHECK(!read.count);}
    }
    for(size_t i=0;i<m;i++)CHECK(!bta_state_decode(history,i,cfg,&read));
    uint8_t wrong[32]={0};CHECK(!bta_state_decode(history,m,wrong,&read));
    history[52]=1;bsr_sha256(history,m-32,history+m-32);CHECK(!bta_state_decode(history,m,cfg,&read));
    blob[4]=1;CHECK(!bta_database_decode(blob,n,&decoded));CHECK(!decoded.count);
}
typedef struct {uint8_t data[BTA_DB_MAX];size_t size;bool exists;} File;
typedef struct {File files[BTA_PATHS];unsigned events,cut;bool stopped,partial;} Mock;
static int rd(void *ctx,enum BtaPath path,uint8_t *out,size_t cap,size_t *n){
    Mock *m=ctx;if(m->stopped)return -1;File *f=&m->files[path];*n=0;if(!f->exists)return 0;if(f->size>cap)return -1;
    memcpy(out,f->data,f->size);*n=f->size;return 1;
}
static bool cut(Mock *m){m->events++;if(m->cut==m->events){m->stopped=true;return true;}return false;}
static bool wr(void *ctx,enum BtaPath path,const uint8_t *data,size_t n){
    Mock *m=ctx;if(m->stopped||m->files[path].exists||n>BTA_DB_MAX)return false;
    if(cut(m)){
        if(m->partial){m->files[path].exists=true;m->files[path].size=n/2;memcpy(m->files[path].data,data,n/2);}return false;
    }
    m->files[path].exists=true;m->files[path].size=n;memcpy(m->files[path].data,data,n);return true;
}
static bool mv(void *ctx,enum BtaPath a,enum BtaPath b){
    Mock *m=ctx;if(m->stopped||!m->files[a].exists||m->files[b].exists||cut(m))return false;
    m->files[b]=m->files[a];memset(&m->files[a],0,sizeof(File));return true;
}
static bool rm(void *ctx,enum BtaPath path){
    Mock *m=ctx;if(m->stopped||!m->files[path].exists||cut(m))return false;
    memset(&m->files[path],0,sizeof(File));return true;
}
static void transaction_tests(void){
    BthRecords dest,src,out;BtaState before,after;BtaStats stats;fixture(&dest,&src,&before);
    fill_record(src.record[src.count++],99,"Joy-Con (L)",99);CHECK(bta_merge(&dest,&src,&before,&out,&after,&stats));
    uint8_t old_db[BTA_DB_MAX],new_db[BTA_DB_MAX],old_state[BTA_STATE_MAX],new_state[BTA_STATE_MAX];size_t a,b,c,d;
    CHECK(bta_database_encode(&dest,old_db,sizeof(old_db),&a));CHECK(bta_database_encode(&out,new_db,sizeof(new_db),&b));
    CHECK(bta_state_encode(&before,cfg,old_state,sizeof(old_state),&c));CHECK(bta_state_encode(&after,cfg,new_state,sizeof(new_state),&d));
    Mock *m=calloc(1,sizeof(*m));CHECK(m!=NULL);BtaIo io={m,rd,wr,mv,rm};
    for(unsigned partial=0;partial<2;partial++)for(unsigned stop=0;stop<=9;stop++){
        memset(m,0,sizeof(*m));m->files[BTA_DB].exists=m->files[BTA_STATE].exists=true;m->files[BTA_DB].size=a;m->files[BTA_STATE].size=c;
        memcpy(m->files[BTA_DB].data,old_db,a);memcpy(m->files[BTA_STATE].data,old_state,c);m->cut=stop;m->partial=partial!=0;
        bool committed=bta_transaction(&io,cfg,old_db,a,old_state,c,new_db,b,new_state,d);
        if(stop==0||stop>=9)CHECK(committed);else CHECK(!committed);
        m->stopped=false;m->cut=0;CHECK(bta_recover(&io,cfg));
        bool old=m->files[BTA_DB].size==a&&!memcmp(m->files[BTA_DB].data,old_db,a)&&m->files[BTA_STATE].size==c&&!memcmp(m->files[BTA_STATE].data,old_state,c);
        bool fresh=m->files[BTA_DB].size==b&&!memcmp(m->files[BTA_DB].data,new_db,b)&&m->files[BTA_STATE].size==d&&!memcmp(m->files[BTA_STATE].data,new_state,d);
        CHECK(old||fresh);CHECK(!m->files[BTA_TXN].exists);CHECK(bta_recover(&io,cfg));
    }
    /* Unexpected post-crash modification must not be replaced with guessed data. */
    memset(m,0,sizeof(*m));m->files[BTA_DB].exists=m->files[BTA_STATE].exists=true;m->files[BTA_DB].size=a;m->files[BTA_STATE].size=c;
    memcpy(m->files[BTA_DB].data,old_db,a);memcpy(m->files[BTA_STATE].data,old_state,c);m->cut=8;
    CHECK(!bta_transaction(&io,cfg,old_db,a,old_state,c,new_db,b,new_state,d));m->stopped=false;m->cut=0;
    m->files[BTA_DB].data[8+0x29]^=1;uint8_t changed=m->files[BTA_DB].data[8+0x29];
    CHECK(!bta_recover(&io,cfg));CHECK(m->files[BTA_DB].data[8+0x29]==changed);
    bth_wipe(m,sizeof(*m));free(m);
}
int main(void){policy_tests();codec_tests();transaction_tests();printf("Automatic sync: %u checks passed; delta history, Hack preservation, capacity, corruption and transaction recovery (synthetic)\n",checks);return 0;}
