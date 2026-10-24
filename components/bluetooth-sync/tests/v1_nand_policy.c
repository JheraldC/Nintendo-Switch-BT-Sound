/* SPDX-License-Identifier: GPL-2.0-only */
#include "nand_policy.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint64_t le64(const uint8_t *p) { return le32(p)|((uint64_t)le32(p+4)<<32); }
static bool used(const uint8_t *p) {
    uint8_t any=0;for(size_t i=0;i<16;i++)any|=p[i];return any!=0;
}
uint32_t btd_crc32(const uint8_t *p,size_t size) {
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<size;i++) {
        crc^=p[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static bool system_name(const uint8_t *p) {
    const char name[]="SYSTEM";
    for(size_t i=0;i<36;i++)if(p[2*i]!=(i<6?(uint8_t)name[i]:0)||p[2*i+1])return false;
    return true;
}
bool btd_parse_gpt(const uint8_t *d,size_t size,uint32_t capacity,BtdWindow *out) {
    if(!out)return false;
    *out=(BtdWindow){0};
    if(!d||size!=BTD_GPT_BYTES||capacity<68||memcmp(d,"EFI PART",8)!=0||
       le32(d+8)!=0x10000||le32(d+12)!=92||le32(d+20)!=0||le64(d+24)!=1||
       le64(d+32)!=(uint64_t)capacity-1||le64(d+40)<34||
       le64(d+48)<le64(d+40)||le64(d+48)>=(uint64_t)capacity-33||
       le64(d+72)!=2||le32(d+80)!=128||le32(d+84)!=128)return false;
    uint8_t header[92];memcpy(header,d,sizeof(header));memset(header+16,0,4);
    if(btd_crc32(header,sizeof(header))!=le32(d+16)||btd_crc32(d+512,128*128)!=le32(d+88))return false;
    unsigned found=0;BtdWindow selected={0};
    for(unsigned i=0;i<128;i++) {
        const uint8_t *entry=d+512+i*128;if(!used(entry))continue;
        uint64_t start=le64(entry+32),end=le64(entry+40);
        if(start<le64(d+40)||start>end||end>le64(d+48)||end>UINT32_MAX)return false;
        for(unsigned j=0;j<i;j++) {
            const uint8_t *other=d+512+j*128;
            if(used(other)&&start<=le64(other+40)&&le64(other+32)<=end)return false;
        }
        if(system_name(entry+56)) {
            if(++found!=1||i!=9||((end-start+1)%32)!=0||end-start+1<32)return false;
            selected=(BtdWindow){(uint32_t)start,(uint32_t)end,true};
        }
    }
    if(found!=1)return false;
    *out=selected;return true;
}
bool btd_read_allowed(const BtdWindow *w,uint32_t first,uint32_t count) {
    if(!w||!count)return false;
    if(!w->ready)return first==BTD_GPT_SECTOR&&count==BTD_GPT_SECTORS;
    return first>=w->start&&first<=w->end&&(uint64_t)count<=(uint64_t)w->end-first+1;
}
bool btd_command_allowed(bool emmc,uint32_t cmd,uint32_t arg,bool req,
                         bool write,uint32_t blocks,uint32_t size,const BtdWindow *w) {
    if(req&&write)return false;
    switch(cmd) {
        case 24:case 25:case 26:case 27:case 28:case 29:case 32:case 33:
        case 35:case 36:case 38:case 42:case 56:return false;
    }
    if(emmc&&cmd==6) {
        unsigned mode=arg>>24,index=(arg>>16)&255,value=(arg>>8)&255;
        if(mode!=3||(arg&255))return false;
        /* No PART_CONFIG writes, even to select the user area. The native
           initializer verifies the already-selected area from EXT_CSD. */
        if(index==179)return false;
        if(index==183)return value==0||value==1||value==2||value==5||value==6;
        if(index==185)return value<=3;
        return false;
    }
    if(emmc&&(cmd==17||cmd==18))return req&&!write&&size==512&&btd_read_allowed(w,arg,blocks);
    if(emmc) {
        switch(cmd) {
            case 0:case 1:case 2:case 3:case 5:case 7:case 8:case 9:
            case 12:case 13:case 16:case 21:case 23:return true;
            default:return false;
        }
    }
    switch(cmd) {
        case 0:case 2:case 3:case 6:case 7:case 8:case 9:case 11:case 12:
        case 13:case 16:case 17:case 18:case 19:case 23:case 41:case 51:
        case 55:case 58:return true;
        default:return false;
    }
}
