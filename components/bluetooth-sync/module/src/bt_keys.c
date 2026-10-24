/* SPDX-License-Identifier: GPL-2.0-only */
#include "bt_keys.h"
#include "bt_only.h"
#include <string.h>
static bool space(unsigned char c) { return c==' '||c=='\t'||c=='\r'; }
static int hex(unsigned char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
static bool nonzero(const uint8_t *p,size_t size) {
    uint8_t any=0;for(size_t i=0;i<size;i++)any|=p[i];return any!=0;
}
static BtoKeyResult line_parse(const uint8_t *line,size_t size,bool overflow,
                               BtoKeys *keys,unsigned *seen) {
    size_t i=0;while(i<size&&space(line[i]))i++;
    size_t start=i;
    while(i<size&&!space(line[i])&&line[i]!='=')i++;
    size_t length=i-start;unsigned flag=0;uint8_t *value=NULL;size_t bytes=0;
    if(length==10&&memcmp(line+start,"bis_key_02",10)==0) {
        flag=1;value=keys->bis_system;bytes=32;
    } else if(length==12&&memcmp(line+start,"save_mac_key",12)==0) {
        flag=2;value=keys->save_mac;bytes=16;
    } else return BTO_KEYS_OK;
    if(*seen&flag)return BTO_KEYS_DUPLICATE;
    if(overflow)return BTO_KEYS_FORMAT;
    while(i<size&&space(line[i]))i++;
    if(i==size||line[i++]!='=')return BTO_KEYS_FORMAT;
    while(i<size&&space(line[i]))i++;
    if(size-i<bytes*2)return BTO_KEYS_FORMAT;
    for(size_t j=0;j<bytes;j++) {
        int a=hex(line[i++]),b=hex(line[i++]);
        if(a<0||b<0)return BTO_KEYS_FORMAT;
        value[j]=(uint8_t)((a<<4)|b);
    }
    while(i<size&&space(line[i]))i++;
    if(i<size&&line[i]!='#'&&line[i]!=';')return BTO_KEYS_FORMAT;
    if(!nonzero(value,bytes)||(flag==1&&
        (!nonzero(value,16)||!nonzero(value+16,16))))return BTO_KEYS_FORMAT;
    *seen|=flag;return BTO_KEYS_OK;
}
BtoKeyResult bto_keys_parse(BtoKeyRead read,void *context,size_t size,BtoKeys *out) {
    if(!out)return BTO_KEYS_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    if(!read||!size||size>BTO_KEYS_FILE_MAX)return BTO_KEYS_ARGUMENT;
    BtoKeys keys={0};uint8_t chunk[128]={0},line[128]={0};
    size_t used=0,done=0;unsigned seen=0;bool overflow=false;
    BtoKeyResult result=BTO_KEYS_OK;
    while(done<size&&result==BTO_KEYS_OK) {
        size_t n=size-done;if(n>sizeof(chunk))n=sizeof(chunk);
        if(!read(context,chunk,n)){result=BTO_KEYS_IO;break;}
        size_t first=0;
        if(done==0&&n>=3&&chunk[0]==0xef&&chunk[1]==0xbb&&chunk[2]==0xbf)first=3;
        for(size_t i=first;i<n;i++) {
            if(chunk[i]==0){result=BTO_KEYS_FORMAT;break;}
            if(chunk[i]=='\n') {
                result=line_parse(line,used,overflow,&keys,&seen);
                bto_wipe(line,sizeof(line));used=0;overflow=false;
                if(result!=BTO_KEYS_OK)break;
            } else if(used==0&&space(chunk[i]))continue;
            else if(used<sizeof(line))line[used++]=chunk[i];
            else overflow=true;
        }
        done+=n;bto_wipe(chunk,sizeof(chunk));
    }
    if(result==BTO_KEYS_OK&&used)result=line_parse(line,used,overflow,&keys,&seen);
    if(result==BTO_KEYS_OK&&seen!=3)result=BTO_KEYS_MISSING;
    if(result==BTO_KEYS_OK)memcpy(out,&keys,sizeof(keys));
    bto_wipe(&keys,sizeof(keys));bto_wipe(chunk,sizeof(chunk));bto_wipe(line,sizeof(line));
    return result;
}
