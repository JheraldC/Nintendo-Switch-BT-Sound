/* SPDX-License-Identifier: GPL-2.0-only */
#include "bt_only.h"
#include <string.h>
enum { OFF_KEY=0x29,OFF_PRESENT=0x39,OFF_VID=0x40,OFF_PID=0x42,
       OFF_DESCRIPTOR_SIZE=0x46,OFF_BRR_SIZE=0xCA,OFF_NAME=0xD6,NAME_SIZE=0xF9 };
_Static_assert(sizeof(BtoRecord)==0x200,"Bluetooth record size");
void bto_wipe(void *p,size_t n) {
    volatile uint8_t *q=p;
    if(q!=NULL)while(n--)*q++=0;
}
static uint16_t u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8)); }
static uint64_t u64(const uint8_t *p) {
    uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;
}
static bool filled(const uint8_t *p,size_t n,uint8_t value) {
    for(size_t i=0;i<n;i++) {
        if(p[i]!=value)return false;
    }
    return true;
}
static bool record_valid(const BtoRecord *r) {
    return !filled(r->bytes,6,0)&&!filled(r->bytes,6,0xFF)
        &&memchr(r->bytes+OFF_NAME,0,NAME_SIZE)!=NULL
        &&u16(r->bytes+OFF_DESCRIPTOR_SIZE)<=0x80
        &&u16(r->bytes+OFF_BRR_SIZE)<=9;
}
BtoResult bto_validate(const BtoDatabase *db) {
    if(db==NULL)return BTO_ARGUMENT;
    if(db->count>BTO_MAX_RECORDS)return BTO_COUNT;
    for(size_t i=0;i<db->count;i++) {
        if(!record_valid(&db->records[i]))return BTO_FORMAT;
        for(size_t j=0;j<i;j++)
            if(memcmp(db->records[i].bytes,db->records[j].bytes,6)==0)return BTO_DUPLICATE;
    }
    return BTO_OK;
}
BtoResult bto_decode(const uint8_t *data,size_t size,BtoDatabase *out) {
    if(out==NULL)return BTO_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    if(data==NULL||size<8)return BTO_FORMAT;
    uint64_t count=u64(data);
    if(count>BTO_MAX_RECORDS)return BTO_COUNT;
    if(size!=8+(size_t)count*BTO_RECORD_SIZE)return BTO_FORMAT;
    out->count=(size_t)count;
    memcpy(out->records,data+8,out->count*BTO_RECORD_SIZE);
    BtoResult result=bto_validate(out);
    if(result!=BTO_OK)bto_wipe(out,sizeof(*out));
    return result;
}
BtoResult bto_encode(const BtoDatabase *db,uint8_t *out,size_t capacity,size_t *written) {
    if(written==NULL)return BTO_ARGUMENT;
    *written=0;
    if(out==NULL)return BTO_ARGUMENT;
    BtoResult result=bto_validate(db);
    if(result!=BTO_OK)return result;
    size_t needed=8+db->count*BTO_RECORD_SIZE;
    if(capacity<needed)return BTO_CAPACITY;
    uint64_t count=(uint64_t)db->count;
    for(unsigned i=0;i<8;i++)out[i]=(uint8_t)(count>>(8*i));
    memcpy(out+8,db->records,db->count*BTO_RECORD_SIZE);
    *written=needed;return BTO_OK;
}
static BtoResult is_nintendo(const BtoRecord *r) {
    if(u16(r->bytes+OFF_VID)!=0x057E)return BTO_NOT_NINTENDO;
    const char *name=NULL;
    switch(u16(r->bytes+OFF_PID)) {
        case 0x2006:name="Joy-Con (L)";break;
        case 0x2007:name="Joy-Con (R)";break;
        case 0x2009:name="Pro Controller";break;
        default:return BTO_NOT_NINTENDO;
    }
    if(strcmp((const char *)r->bytes+OFF_NAME,name)!=0)return BTO_NOT_NINTENDO;
    if(r->bytes[OFF_PRESENT]!=1||filled(r->bytes+OFF_KEY,16,0))return BTO_NO_LINK_KEY;
    return BTO_OK;
}
BtoResult bto_nintendo_type(const BtoRecord *r,BtoNintendoType *out) {
    if(out==NULL)return BTO_ARGUMENT;
    *out=BTO_NINTENDO_NONE;
    if(r==NULL)return BTO_ARGUMENT;
    if(!record_valid(r)||r->bytes[OFF_PRESENT]>1)return BTO_FORMAT;
    BtoResult result=is_nintendo(r);if(result!=BTO_OK)return result;
    switch(u16(r->bytes+OFF_PID)) {
        case 0x2006:*out=BTO_NINTENDO_JOY_L;break;
        case 0x2007:*out=BTO_NINTENDO_JOY_R;break;
        case 0x2009:*out=BTO_NINTENDO_PRO;break;
        default:return BTO_NOT_NINTENDO;
    }
    return BTO_OK;
}
BtoResult bto_select_nintendo(const BtoDatabase *source,
    const uint8_t (*addresses)[6],size_t address_count,BtoDatabase *out) {
    if(out==NULL||out==source)return BTO_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    if(addresses==NULL||address_count==0||address_count>BTO_MAX_RECORDS)return BTO_ARGUMENT;
    BtoResult result=bto_validate(source);if(result!=BTO_OK)return result;
    for(size_t a=0;a<address_count;a++) {
        for(size_t j=0;j<a;j++)if(memcmp(addresses[a],addresses[j],6)==0){result=BTO_DUPLICATE;goto fail;}
        const BtoRecord *record=NULL;
        for(size_t i=0;i<source->count;i++)
            if(memcmp(source->records[i].bytes,addresses[a],6)==0)record=&source->records[i];
        if(record==NULL){result=BTO_NOT_FOUND;goto fail;}
        result=is_nintendo(record);if(result!=BTO_OK)goto fail;
        out->records[out->count++]=*record;
    }
    return BTO_OK;
fail:bto_wipe(out,sizeof(*out));return result;
}
BtoResult bto_merge(const BtoDatabase *destination,const BtoDatabase *selected,BtoDatabase *out) {
    if(out==NULL||out==destination||out==selected)return BTO_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    BtoResult result=bto_validate(destination);if(result!=BTO_OK)return result;
    result=bto_validate(selected);if(result!=BTO_OK)return result;
    if(selected->count==0)return BTO_NOT_FOUND;
    for(size_t i=0;i<selected->count;i++)if((result=is_nintendo(&selected->records[i]))!=BTO_OK)return result;
    /* Compute size before copying to keep failure output empty. */
    size_t kept=0;
    for(size_t i=0;i<destination->count;i++) {
        bool replaced=false;
        for(size_t j=0;j<selected->count;j++)
            if(memcmp(destination->records[i].bytes,selected->records[j].bytes,6)==0)replaced=true;
        if(!replaced)kept++;
    }
    if(kept+selected->count>BTO_MAX_RECORDS)return BTO_CAPACITY;
    for(size_t i=0;i<destination->count;i++) {
        bool replaced=false;
        for(size_t j=0;j<selected->count;j++)
            if(memcmp(destination->records[i].bytes,selected->records[j].bytes,6)==0)replaced=true;
        if(!replaced)out->records[out->count++]=destination->records[i];
    }
    for(size_t i=0;i<selected->count;i++)out->records[out->count++]=selected->records[i];
    return BTO_OK;
}
const char *bto_result_name(BtoResult r) {
    static const char *names[]={"OK","ARGUMENT","FORMAT","COUNT","DUPLICATE",
        "NOT_FOUND","NOT_NINTENDO","NO_LINK_KEY","CAPACITY"};
    return (unsigned)r<sizeof(names)/sizeof(names[0])?names[r]:"UNKNOWN";
}
