/* SPDX-License-Identifier: GPL-2.0-only */
#include "auto_sync.h"
#include "sha256.h"
#include <string.h>
_Static_assert(sizeof(BtaSeen)==40,"history ABI");
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t n){for(size_t i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static bool nonzero(const uint8_t *p,size_t n){while(n--)if(*p++)return true;return false;}
bool bta_controller(const uint8_t r[512]){
    if(!r||!memchr(r+0xD6,0,0xF9)||r[0x39]!=1||!nonzero(r+0x29,16)||(r[0x27]&0x1F)!=5)return false;
    return !strcmp((const char *)r+0xD6,"Pro Controller")||!strcmp((const char *)r+0xD6,"Joy-Con (L)")||!strcmp((const char *)r+0xD6,"Joy-Con (R)");
}
static bool state_valid(const BtaState *s){
    if(!s||s->count>BTH_MAX)return false;
    for(size_t i=0;i<s->count;i++){
        const BtaSeen *e=&s->entries[i];
        if(!nonzero(e->address,6)||e->reserved||e->controller>1||!nonzero(e->fingerprint,32))return false;
        for(size_t j=0;j<i;j++)if(!memcmp(e->address,s->entries[j].address,6))return false;
    }
    return true;
}
bool bta_state_from_source(const BthRecords *source,BtaState *out){
    if(!out)return false;
    bth_wipe(out,sizeof(*out));if(!bth_records_valid(source))return false;
    uint8_t pairing[23]={0};out->count=(uint32_t)source->count;
    for(size_t i=0;i<source->count;i++){
        const uint8_t *r=source->record[i];BtaSeen *e=&out->entries[i];
        memcpy(e->address,r,6);e->controller=bta_controller(r)?1:0;
        memcpy(pairing,r,6);memcpy(pairing+6,r+0x29,17);bsr_sha256(pairing,sizeof(pairing),e->fingerprint);
    }
    for(size_t i=1;i<out->count;i++)for(size_t j=i;j>0&&memcmp(out->entries[j-1].address,out->entries[j].address,6)>0;j--){
        BtaSeen temporary=out->entries[j-1];out->entries[j-1]=out->entries[j];out->entries[j]=temporary;bth_wipe(&temporary,sizeof(temporary));
    }
    bth_wipe(pairing,sizeof(pairing));return true;
}
bool bta_state_encode(const BtaState *s,const uint8_t cfg[32],uint8_t *out,size_t cap,size_t *written){
    if(written)*written=0;
    if(!state_valid(s)||!cfg||!out||!written)return false;
    size_t n=96+s->count*40;if(n>cap)return false;
    memset(out,0,n);memcpy(out,"BTASYN1",7);put32(out+8,1);put32(out+12,s->count);memcpy(out+16,cfg,32);put32(out+48,0x220000);
    memcpy(out+64,s->entries,s->count*40);bsr_sha256(out,n-32,out+n-32);*written=n;return true;
}
bool bta_state_decode(const uint8_t *p,size_t n,const uint8_t cfg[32],BtaState *out){
    if(!out)return false;
    bth_wipe(out,sizeof(*out));bool ok=false;uint8_t hash[32]={0};
    if(!p||!cfg||n<96||n>BTA_STATE_MAX||memcmp(p,"BTASYN1\0",8)||le32(p+8)!=1||le32(p+12)>BTH_MAX||memcmp(p+16,cfg,32)||le32(p+48)!=0x220000)goto done;
    size_t count=le32(p+12);if(n!=96+40*count)goto done;
    for(size_t i=52;i<64;i++)if(p[i])goto done;
    bsr_sha256(p,n-32,hash);if(memcmp(hash,p+n-32,32))goto done;
    out->count=(uint32_t)count;memcpy(out->entries,p+64,count*40);ok=state_valid(out);
done:
    bth_wipe(hash,sizeof(hash));if(!ok)bth_wipe(out,sizeof(*out));return ok;
}
bool bta_database_encode(const BthRecords *r,uint8_t *out,size_t cap,size_t *written){
    if(written)*written=0;
    if(!bth_records_valid(r)||!out||!written)return false;
    size_t n=8+r->count*512;if(n>cap)return false;
    memset(out,0,8);put32(out,(uint32_t)r->count);memcpy(out+8,r->record,r->count*512);*written=n;return true;
}
bool bta_database_decode(const uint8_t *p,size_t n,BthRecords *out){
    if(!out)return false;
    bth_wipe(out,sizeof(*out));
    if(!p||n<8||n>BTA_DB_MAX||le32(p)>BTH_MAX||le32(p+4)!=0||n!=8+(size_t)le32(p)*512)return false;
    out->count=le32(p);memcpy(out->record,p+8,out->count*512);
    if(!bth_records_valid(out)){bth_wipe(out,sizeof(*out));return false;}return true;
}
bool bta_merge(const BthRecords *destination,const BthRecords *source,const BtaState *before,BthRecords *out,BtaState *after,BtaStats *stats){
    if(!out||!after||!stats||out==destination||out==source||before==after)return false;
    bth_wipe(out,sizeof(*out));bth_wipe(after,sizeof(*after));bth_wipe(stats,sizeof(*stats));
    if(!bth_records_valid(destination)||!bth_records_valid(source)||!state_valid(before)||!bta_state_from_source(source,after))return false;
    memcpy(out,destination,sizeof(*out));
    for(size_t i=0;i<source->count;i++){
        const BtaSeen *now=NULL,*previous=NULL;const uint8_t *r=source->record[i];
        for(size_t j=0;j<after->count;j++)if(!memcmp(r,after->entries[j].address,6)){now=&after->entries[j];break;}
        if(!now)goto failure;
        if(!now->controller){stats->ignored++;continue;}
        for(size_t j=0;j<before->count;j++)if(!memcmp(now->address,before->entries[j].address,6)){previous=&before->entries[j];break;}
        /* Hack may have changed its own pairing. Do not overwrite it unless
         * Original has a NEW pairing since the last successfully recorded view. */
        bool changed=!previous||!previous->controller||memcmp(now->fingerprint,previous->fingerprint,32)!=0;
        if(!changed)continue;
        size_t found=out->count;
        for(size_t j=0;j<out->count;j++)if(!memcmp(r,out->record[j],6)){found=j;break;}
        if(found<out->count){
            if(memcmp(r,out->record[found],512)){memcpy(out->record[found],r,512);stats->updated++;}
        } else {
            if(out->count==BTH_MAX)goto failure;
            memcpy(out->record[out->count++],r,512);stats->added++;
        }
    }
    stats->database_changed=stats->added||stats->updated;
    stats->history_changed=before->count!=after->count||memcmp(before->entries,after->entries,after->count*40)!=0;
    return true;
failure:
    bth_wipe(out,sizeof(*out));bth_wipe(after,sizeof(*after));bth_wipe(stats,sizeof(*stats));return false;
}
