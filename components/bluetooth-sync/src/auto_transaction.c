/* SPDX-License-Identifier: GPL-2.0-only */
#include "auto_sync.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
typedef struct {uint8_t old_db[32],new_db[32],old_state[32],new_state[32];uint32_t sizes[4];} Txn;
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t n){for(size_t i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void hash(const void *p,size_t n,uint8_t h[32]){bsr_sha256(p,n,h);}
static bool valid_io(const BtaIo *io){return io&&io->read&&io->write_new&&io->rename&&io->remove;}
static bool txn_decode(const uint8_t p[BTA_TXN_SIZE],const uint8_t cfg[32],Txn *out){
    uint8_t h[32]={0};bool ok=false;memset(out,0,sizeof(*out));
    if(memcmp(p,"BTATXN1\0",8)||le32(p+8)!=1||le32(p+12)!=0||memcmp(p+16,cfg,32)||le32(p+192)!=0x220000)goto done;
    for(size_t i=196;i<224;i++)if(p[i])goto done;
    hash(p,224,h);if(memcmp(h,p+224,32))goto done;
    memcpy(out,p+48,128);for(size_t i=0;i<4;i++)out->sizes[i]=le32(p+176+i*4);
    ok=out->sizes[0]>=8&&out->sizes[0]<=BTA_DB_MAX&&out->sizes[1]>=8&&out->sizes[1]<=BTA_DB_MAX
        &&out->sizes[2]>=96&&out->sizes[2]<=BTA_STATE_MAX&&out->sizes[3]>=96&&out->sizes[3]<=BTA_STATE_MAX;
done:
    bth_wipe(h,sizeof(h));if(!ok)bth_wipe(out,sizeof(*out));return ok;
}
static bool matches(const BtaIo *io,enum BtaPath path,const uint8_t expected[32],size_t expected_size,uint8_t *scratch){
    size_t n=0;uint8_t h[32]={0};bool ok=io->read(io->context,path,scratch,BTA_DB_MAX,&n)==1&&n==expected_size;
    if(ok){hash(scratch,n,h);ok=memcmp(h,expected,32)==0;}
    bth_wipe(h,sizeof(h));bth_wipe(scratch,BTA_DB_MAX);return ok;
}
static int present(const BtaIo *io,enum BtaPath path,uint8_t *scratch){
    size_t n=0;int result=io->read(io->context,path,scratch,BTA_DB_MAX,&n);bth_wipe(scratch,BTA_DB_MAX);return result;
}
static bool remove_if_present(const BtaIo *io,enum BtaPath path,uint8_t *scratch){
    int found=present(io,path,scratch);return found==0||(found==1&&io->remove(io->context,path));
}
static bool mains_valid(const BtaIo *io,const uint8_t cfg[32],uint8_t *scratch){
    BthRecords *records=calloc(1,sizeof(*records));BtaState state;size_t n=0;bool ok=false;
    if(!records)return false;
    if(io->read(io->context,BTA_DB,scratch,BTA_DB_MAX,&n)!=1||!bta_database_decode(scratch,n,records))goto done;
    if(io->read(io->context,BTA_STATE,scratch,BTA_DB_MAX,&n)!=1||!bta_state_decode(scratch,n,cfg,&state))goto done;
    ok=true;
done:
    bth_wipe(records,sizeof(*records));free(records);bth_wipe(&state,sizeof(state));bth_wipe(scratch,BTA_DB_MAX);return ok;
}
static bool restore_one(const BtaIo *io,enum BtaPath main,enum BtaPath next,enum BtaPath previous,const uint8_t old[32],size_t old_size,const uint8_t new_hash[32],size_t new_size,uint8_t *scratch){
    if(matches(io,main,old,old_size,scratch))return true;
    int exists=present(io,main,scratch);
    if(exists<0||!matches(io,previous,old,old_size,scratch))return false;
    if(exists==1){
        if(!matches(io,main,new_hash,new_size,scratch)||present(io,next,scratch)!=0||!io->rename(io->context,main,next))return false;
    }
    return io->rename(io->context,previous,main)&&matches(io,main,old,old_size,scratch);
}
bool bta_recover(const BtaIo *io,const uint8_t cfg[32]){
    if(!valid_io(io)||!cfg)return false;
    uint8_t *scratch=malloc(BTA_DB_MAX),packet[BTA_TXN_SIZE]={0};Txn txn={0};size_t n=0;bool ok=false;
    if(!scratch)return false;
    int exists=io->read(io->context,BTA_TXN,packet,sizeof(packet),&n);
    if(exists<0)goto done;
    if(exists==0){
        if(!mains_valid(io,cfg,scratch))goto done;
        ok=remove_if_present(io,BTA_DB_NEXT,scratch)&&remove_if_present(io,BTA_STATE_NEXT,scratch);goto done;
    }
    if(n!=sizeof(packet)||!txn_decode(packet,cfg,&txn)){
        /* An interrupted journal write precedes all renames. Refuse ambiguous
         * cases with backups instead of guessing or deleting a possible alias. */
        if(!mains_valid(io,cfg,scratch)||present(io,BTA_DB_PREV,scratch)!=0||present(io,BTA_STATE_PREV,scratch)!=0)goto done;
    } else if(!(matches(io,BTA_DB,txn.new_db,txn.sizes[1],scratch)&&matches(io,BTA_STATE,txn.new_state,txn.sizes[3],scratch))){
        if(!restore_one(io,BTA_DB,BTA_DB_NEXT,BTA_DB_PREV,txn.old_db,txn.sizes[0],txn.new_db,txn.sizes[1],scratch)
           ||!restore_one(io,BTA_STATE,BTA_STATE_NEXT,BTA_STATE_PREV,txn.old_state,txn.sizes[2],txn.new_state,txn.sizes[3],scratch))goto done;
    }
    if(!mains_valid(io,cfg,scratch))goto done;
    ok=remove_if_present(io,BTA_DB_NEXT,scratch)&&remove_if_present(io,BTA_STATE_NEXT,scratch)&&io->remove(io->context,BTA_TXN);
done:
    bth_wipe(scratch,BTA_DB_MAX);free(scratch);bth_wipe(packet,sizeof(packet));bth_wipe(&txn,sizeof(txn));return ok;
}
bool bta_transaction(const BtaIo *io,const uint8_t cfg[32],const uint8_t *old_db,size_t old_db_size,const uint8_t *old_state,size_t old_state_size,const uint8_t *new_db,size_t new_db_size,const uint8_t *new_state,size_t new_state_size){
    if(!valid_io(io)||!cfg||!old_db||!old_state||!new_db||!new_state)return false;
    if(old_db_size<8||old_db_size>BTA_DB_MAX||new_db_size<8||new_db_size>BTA_DB_MAX||old_state_size<96||old_state_size>BTA_STATE_MAX||new_state_size<96||new_state_size>BTA_STATE_MAX)return false;
    uint8_t *scratch=malloc(BTA_DB_MAX),packet[BTA_TXN_SIZE]={0};bool ok=false;Txn txn={0};
    if(!scratch)return false;
    hash(old_db,old_db_size,txn.old_db);hash(new_db,new_db_size,txn.new_db);hash(old_state,old_state_size,txn.old_state);hash(new_state,new_state_size,txn.new_state);
    txn.sizes[0]=(uint32_t)old_db_size;txn.sizes[1]=(uint32_t)new_db_size;txn.sizes[2]=(uint32_t)old_state_size;txn.sizes[3]=(uint32_t)new_state_size;
    if(present(io,BTA_TXN,scratch)!=0||!mains_valid(io,cfg,scratch)||!matches(io,BTA_DB,txn.old_db,old_db_size,scratch)||!matches(io,BTA_STATE,txn.old_state,old_state_size,scratch))goto done;
    for(enum BtaPath path=BTA_DB_NEXT;path<=BTA_STATE_PREV;path++)if(!remove_if_present(io,path,scratch))goto done;
    if(!io->write_new(io->context,BTA_DB_NEXT,new_db,new_db_size)||!matches(io,BTA_DB_NEXT,txn.new_db,new_db_size,scratch))goto done;
    if(!io->write_new(io->context,BTA_STATE_NEXT,new_state,new_state_size)||!matches(io,BTA_STATE_NEXT,txn.new_state,new_state_size,scratch))goto done;
    memcpy(packet,"BTATXN1",7);put32(packet+8,1);memcpy(packet+16,cfg,32);memcpy(packet+48,&txn,128);
    for(size_t i=0;i<4;i++)put32(packet+176+i*4,txn.sizes[i]);
    put32(packet+192,0x220000);hash(packet,224,packet+224);
    if(!io->write_new(io->context,BTA_TXN,packet,sizeof(packet)))goto done;
    uint8_t journal_hash[32];hash(packet,sizeof(packet),journal_hash);
    bool verified=matches(io,BTA_TXN,journal_hash,sizeof(packet),scratch);bth_wipe(journal_hash,sizeof(journal_hash));if(!verified)goto done;
    if(!io->rename(io->context,BTA_DB,BTA_DB_PREV)||!io->rename(io->context,BTA_STATE,BTA_STATE_PREV)
       ||!io->rename(io->context,BTA_DB_NEXT,BTA_DB)||!io->rename(io->context,BTA_STATE_NEXT,BTA_STATE))goto done;
    if(!matches(io,BTA_DB,txn.new_db,new_db_size,scratch)||!matches(io,BTA_STATE,txn.new_state,new_state_size,scratch))goto done;
    ok=io->remove(io->context,BTA_TXN);
done:
    bth_wipe(scratch,BTA_DB_MAX);free(scratch);bth_wipe(packet,sizeof(packet));bth_wipe(&txn,sizeof(txn));return ok;
}
