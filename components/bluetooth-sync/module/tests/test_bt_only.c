/* SPDX-License-Identifier: GPL-2.0-only */
#include "bt_only.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(expr) do { ++checks; assert(expr); } while(0)
static BtoRecord make_record(unsigned id,unsigned pid) {
    BtoRecord r={{0}};
    r.bytes[0]=0x98;r.bytes[5]=(uint8_t)id;
    r.bytes[0x29]=(uint8_t)id;r.bytes[0x39]=1;
    r.bytes[0x40]=0x7E;r.bytes[0x41]=5;
    r.bytes[0x42]=(uint8_t)pid;r.bytes[0x43]=(uint8_t)(pid>>8);
    const char *name=pid==0x2006?"Joy-Con (L)":pid==0x2007?"Joy-Con (R)":"Pro Controller";
    memcpy(r.bytes+0xD6,name,strlen(name)+1);return r;
}
static bool empty(const void *p,size_t n) {
    const uint8_t *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;
}
int main(void) {
    BtoDatabase source={0},decoded={0},selected={0},destination={0},merged={0};
    source.count=3;
    source.records[0]=make_record(1,0x2009);source.records[1]=make_record(2,0x2006);source.records[2]=make_record(3,0x2007);
    uint8_t encoded[BTO_DATABASE_MAX+1];size_t written=0;
    CHECK(bto_validate(&source)==BTO_OK);
    CHECK(bto_encode(&source,encoded,sizeof(encoded),&written)==BTO_OK);
    CHECK(written==8+3*512);
    CHECK(encoded[0]==3&&empty(encoded+1,7));
    CHECK(bto_decode(encoded,written,&decoded)==BTO_OK);
    CHECK(memcmp(&source,&decoded,sizeof(source))==0);
    for(size_t size=0;size<written;size++) {
        memset(&decoded,0xAA,sizeof(decoded));
        CHECK(bto_decode(encoded,size,&decoded)!=BTO_OK);
        CHECK(empty(&decoded,sizeof(decoded)));
    }
    CHECK(bto_decode(encoded,written+1,&decoded)==BTO_FORMAT);
    encoded[0]=21;CHECK(bto_decode(encoded,8,&decoded)==BTO_COUNT);encoded[0]=3;
    uint8_t address[2][6];memcpy(address[0],source.records[1].bytes,6);
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_OK);
    CHECK(selected.count==1);
    CHECK(memcmp(&selected.records[0],&source.records[1],sizeof(BtoRecord))==0);
    CHECK(empty(&selected.records[1],sizeof(selected.records)-sizeof(BtoRecord)));
    destination.count=2;destination.records[0]=make_record(4,0x2009);destination.records[1]=source.records[1];
    destination.records[1].bytes[0x29]=99;
    BtoDatabase original_destination=destination,original_source=source;
    CHECK(bto_merge(&destination,&selected,&merged)==BTO_OK);
    CHECK(merged.count==2);
    CHECK(memcmp(&merged.records[0],&destination.records[0],sizeof(BtoRecord))==0);
    CHECK(memcmp(&merged.records[1],&selected.records[0],sizeof(BtoRecord))==0);
    CHECK(memcmp(&destination,&original_destination,sizeof(destination))==0);
    CHECK(memcmp(&source,&original_source,sizeof(source))==0);
    memcpy(address[1],address[0],6);
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,2,&selected)==BTO_DUPLICATE);
    CHECK(empty(&selected,sizeof(selected)));
    address[0][5]=88;
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_NOT_FOUND);
    CHECK(empty(&selected,sizeof(selected)));
    memcpy(address[0],source.records[0].bytes,6);
    source.records[0].bytes[0x40]=0;
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_NOT_NINTENDO);
    CHECK(empty(&selected,sizeof(selected)));source=original_source;
    source.records[0].bytes[0x39]=0;
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_NO_LINK_KEY);
    CHECK(empty(&selected,sizeof(selected)));source=original_source;
    memset(source.records[0].bytes+0x29,0,16);
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_NO_LINK_KEY);source=original_source;
    memset(source.records[0].bytes+0xD6,'X',0xF9);
    CHECK(bto_validate(&source)==BTO_FORMAT);source=original_source;
    source.records[0].bytes[0x46]=0x81;CHECK(bto_validate(&source)==BTO_FORMAT);source=original_source;
    source.records[0].bytes[0xCA]=10;CHECK(bto_validate(&source)==BTO_FORMAT);source=original_source;
    source.records[1]=source.records[0];CHECK(bto_validate(&source)==BTO_DUPLICATE);source=original_source;
    source.count=21;CHECK(bto_validate(&source)==BTO_COUNT);source=original_source;
    CHECK(bto_select_nintendo(&source,(const uint8_t (*)[6])address,1,&selected)==BTO_OK);
    destination.count=20;for(size_t i=0;i<20;i++)destination.records[i]=make_record((unsigned)i+20,0x2009);
    CHECK(bto_merge(&destination,&selected,&merged)==BTO_CAPACITY);
    CHECK(empty(&merged,sizeof(merged)));
    for(size_t cap=0;cap<8+source.count*512;cap++) {
        memset(encoded,0x5A,sizeof(encoded));written=123;
        CHECK(bto_encode(&source,encoded,cap,&written)==BTO_CAPACITY);
        CHECK(written==0&&encoded[0]==0x5A);
    }
    /* Malformed arbitrary bytes never escape as partially decoded records. */
    uint32_t seed=0x6A09E667;
    for(unsigned t=0;t<1000;t++) {
        size_t size=t%sizeof(encoded);
        for(size_t i=0;i<size;i++){seed=seed*1664525U+1013904223U;encoded[i]=(uint8_t)(seed>>24);}
        BtoResult r=bto_decode(encoded,size,&decoded);
        CHECK(r!=BTO_OK||bto_validate(&decoded)==BTO_OK);
        if(r!=BTO_OK)CHECK(empty(&decoded,sizeof(decoded)));
    }
    bto_wipe(&source,sizeof(source));bto_wipe(&destination,sizeof(destination));
    bto_wipe(&decoded,sizeof(decoded));bto_wipe(&selected,sizeof(selected));
    bto_wipe(&merged,sizeof(merged));bto_wipe(encoded,sizeof(encoded));
    CHECK(empty(&source,sizeof(source))&&empty(encoded,sizeof(encoded)));
    printf("bt-only: %u checks passed (synthetic PC data only)\n",checks);
    return 0;
}
