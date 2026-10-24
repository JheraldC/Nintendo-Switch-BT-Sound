#include "transfer.h"
#include <string.h>
void bth_wipe(void *p,size_t n){volatile uint8_t *q=p;if(q)while(n--)*q++=0;}
static uint16_t load_le16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t load_le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t load_le64(const uint8_t *p){return load_le32(p)|((uint64_t)load_le32(p+4)<<32);}
static void put(uint8_t *p,uint64_t n,size_t bytes){for(size_t i=0;i<bytes;i++)p[i]=(uint8_t)(n>>(8*i));}
static bool same(const uint8_t *p,size_t n,uint8_t v){for(size_t i=0;i<n;i++)if(p[i]!=v)return false;return true;}
bool bth_records_valid(const BthRecords *r){
    if(!r||r->count>BTH_MAX)return false;
    for(size_t i=0;i<r->count;i++){
        const uint8_t *p=r->record[i];
        if(same(p,6,0)||same(p,6,255)||!memchr(p+0xD6,0,0xF9)||load_le16(p+0x46)>0x80||load_le16(p+0xCA)>9||p[0x39]>1)return false;
        for(size_t j=0;j<i;j++)if(!memcmp(p,r->record[j],6))return false;
    }
    return true;
}
bool bth_packet_build(const BthRecords *r,uint32_t hos,const uint8_t cfg[32],BthHash hash,uint8_t *out,size_t cap,size_t *written){
    if(written)*written=0;
    if(!r||!cfg||!hash||!out||!written||!bth_records_valid(r)||hos!=0x170001)return false;
    size_t n=96+r->count*BTH_RECORD;if(cap<n)return false;
    memset(out,0,n);memcpy(out,"BTHACK1",7);put(out+8,1,4);put(out+12,r->count,4);
    put(out+16,hos,4);put(out+20,1,4);memcpy(out+24,cfg,32);put(out+56,n,8);
    memcpy(out+64,r->record,r->count*BTH_RECORD);hash(out,n-32,out+n-32);*written=n;return true;
}
bool bth_packet_parse(const uint8_t *p,size_t n,uint32_t hos,const uint8_t cfg[32],BthHash hash,BthRecords *out){
    if(!out)return false;
    bth_wipe(out,sizeof(*out));uint8_t digest[32]={0};bool ok=false;
    if(!p||!cfg||!hash||n<96||n>BTH_PACKET_MAX||hos!=0x170001)goto done;
    if(memcmp(p,"BTHACK1\0",8)||load_le32(p+8)!=1||load_le32(p+12)>BTH_MAX||load_le32(p+16)!=hos||load_le32(p+20)!=1||memcmp(p+24,cfg,32)||load_le64(p+56)!=n)goto done;
    size_t count=load_le32(p+12);if(n!=96+count*BTH_RECORD)goto done;
    hash(p,n-32,digest);if(memcmp(digest,p+n-32,32))goto done;
    out->count=count;memcpy(out->record,p+64,count*BTH_RECORD);ok=bth_records_valid(out);
done:
    bth_wipe(digest,sizeof(digest));if(!ok)bth_wipe(out,sizeof(*out));return ok;
}
bool bth_merge_selected(const BthRecords *dest,const BthRecords *src,const size_t *slots,size_t count,BthRecords *out){
    if(!out||out==dest||out==src)return false;
    bth_wipe(out,sizeof(*out));
    if(!bth_records_valid(dest)||!bth_records_valid(src)||!slots||!count||count>BTH_MAX)return false;
    for(size_t i=0;i<count;i++){
        if(slots[i]>=src->count)return false;
        const uint8_t *r=src->record[slots[i]];
        if(r[0x39]!=1||same(r+0x29,16,0))return false;
        for(size_t j=0;j<i;j++)if(slots[i]==slots[j])return false;
    }
    size_t kept=0;
    for(size_t i=0;i<dest->count;i++){
        bool replace=false;for(size_t j=0;j<count;j++)if(!memcmp(dest->record[i],src->record[slots[j]],6))replace=true;
        if(!replace)kept++;
    }
    if(kept+count>BTH_MAX)return false;
    for(size_t i=0;i<dest->count;i++){
        bool replace=false;for(size_t j=0;j<count;j++)if(!memcmp(dest->record[i],src->record[slots[j]],6))replace=true;
        if(!replace)memcpy(out->record[out->count++],dest->record[i],BTH_RECORD);
    }
    for(size_t j=0;j<count;j++)memcpy(out->record[out->count++],src->record[slots[j]],BTH_RECORD);
    return true;
}
