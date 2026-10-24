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
    for(size_t i=0;i<6;i++)if(p[2*i]!=(uint8_t)name[i]||p[2*i+1])return false;
    return p[12]==0&&p[13]==0; /* Exact NUL-terminated UTF-16 name; padding is not the name. */
}
static void problem(BtdGptInfo *info,BtdGptResult result) {
    info->problems|=UINT32_C(1)<<(unsigned)(result-BTD_GPT_ARGUMENT);
}
BtdGptResult btd_check_gpt(const uint8_t *d,size_t size,uint32_t capacity,
                         BtdWindow *out,BtdGptInfo *info) {
    BtdGptInfo local={0};if(!info)info=&local;
    *info=(BtdGptInfo){.capacity=capacity,.entry_index=UINT32_MAX,.system_index=UINT32_MAX};
    if(out)*out=(BtdWindow){0};
    if(!out||!d||size!=BTD_GPT_BYTES||capacity<68)return BTD_GPT_ARGUMENT;
    if(memcmp(d,"EFI PART",8)!=0)return BTD_GPT_SIGNATURE;
    info->signature_valid=true;info->header_size=le32(d+12);info->entries=le32(d+80);
    info->entry_size=le32(d+84);info->primary_lba=le64(d+24);info->backup_lba=le64(d+32);
    info->first_usable=le64(d+40);info->last_usable=le64(d+48);info->table_lba=le64(d+72);
    if(le32(d+8)!=0x10000)problem(info,BTD_GPT_REVISION);
    if(info->header_size!=92)problem(info,BTD_GPT_HEADER_SIZE);
    if(le32(d+20)!=0)problem(info,BTD_GPT_RESERVED);
    if(info->primary_lba!=1)problem(info,BTD_GPT_PRIMARY_LBA);
    if(info->backup_lba!=(uint64_t)capacity-1)problem(info,BTD_GPT_BACKUP_LBA);
    if(info->first_usable<34||info->last_usable<info->first_usable||
       info->last_usable>=(uint64_t)capacity-33)problem(info,BTD_GPT_USABLE_RANGE);
    if(info->table_lba!=2)problem(info,BTD_GPT_TABLE_LBA);
    bool count_ok=info->entries>=1&&info->entries<=128;
    if(!count_ok)problem(info,BTD_GPT_ENTRY_COUNT);
    if(info->entry_size!=128)problem(info,BTD_GPT_ENTRY_SIZE);
    if(info->header_size==92) {
        uint8_t header[92];memcpy(header,d,sizeof(header));memset(header+16,0,4);
        info->header_crc_checked=true;info->header_crc_ok=btd_crc32(header,sizeof(header))==le32(d+16);
        if(!info->header_crc_ok)problem(info,BTD_GPT_HEADER_CRC);
    }
    BtdWindow selected={0};unsigned found=0;
    if(count_ok&&info->entry_size==128&&info->table_lba==2) {
        /* CRC covers the DECLARED array, not the capacity of our 16 KiB buffer. */
        size_t bytes=(size_t)info->entries*128;
        info->table_crc_checked=true;info->table_crc_ok=btd_crc32(d+512,bytes)==le32(d+88);
        if(!info->table_crc_ok)problem(info,BTD_GPT_TABLE_CRC);
        for(unsigned i=0;i<info->entries;i++) {
            const uint8_t *entry=d+512+i*128;if(!used(entry))continue;
            info->used_entries++;
            uint64_t start=le64(entry+32),end=le64(entry+40);
            bool valid=start>=info->first_usable&&start<=end&&end<=info->last_usable&&end<=UINT32_MAX;
            if(!valid){info->entry_index=i;problem(info,BTD_GPT_PART_RANGE);}
            for(unsigned j=0;j<i;j++) {
                const uint8_t *other=d+512+j*128;
                if(used(other)&&start<=le64(other+40)&&le64(other+32)<=end) {
                    info->entry_index=i;problem(info,BTD_GPT_OVERLAP);
                }
            }
            if(system_name(entry+56)) {
                found++;info->system_index=i;
                if(found>1)problem(info,BTD_GPT_DUPLICATE_SYSTEM);
                if(valid) {
                    info->system_sectors=end-start+1;
                    if(info->system_sectors%32||info->system_sectors<32)problem(info,BTD_GPT_SYSTEM_ALIGNMENT);
                    selected=(BtdWindow){(uint32_t)start,(uint32_t)end,true};
                }
            }
        }
        if(found==0)problem(info,BTD_GPT_SYSTEM_MISSING);
    }
    for(unsigned i=0;i<19;i++)if(info->problems&(UINT32_C(1)<<i))return (BtdGptResult)(BTD_GPT_ARGUMENT+i);
    *out=selected;return BTD_GPT_OK;
}
bool btd_parse_gpt(const uint8_t *d,size_t size,uint32_t capacity,BtdWindow *out) {
    return btd_check_gpt(d,size,capacity,out,NULL)==BTD_GPT_OK;
}
const char *btd_gpt_result_stage(BtdGptResult result) {
    switch(result) {
        case BTD_GPT_OK:return "GPT_SYSTEM";
        case BTD_GPT_ARGUMENT:return "G_ARGUMENT";
        case BTD_GPT_SIGNATURE:return "G_SIGNATURE";
        case BTD_GPT_REVISION:return "G_REVISION";
        case BTD_GPT_HEADER_SIZE:return "G_HEADER_SIZE";
        case BTD_GPT_RESERVED:return "G_RESERVED";
        case BTD_GPT_PRIMARY_LBA:return "G_PRIMARY_LBA";
        case BTD_GPT_BACKUP_LBA:return "G_BACKUP_LBA";
        case BTD_GPT_USABLE_RANGE:return "G_USABLE_RANGE";
        case BTD_GPT_TABLE_LBA:return "G_TABLE_LBA";
        case BTD_GPT_ENTRY_COUNT:return "G_ENTRY_COUNT";
        case BTD_GPT_ENTRY_SIZE:return "G_ENTRY_SIZE";
        case BTD_GPT_HEADER_CRC:return "G_HEADER_CRC";
        case BTD_GPT_TABLE_CRC:return "G_TABLE_CRC";
        case BTD_GPT_PART_RANGE:return "G_PART_RANGE";
        case BTD_GPT_OVERLAP:return "G_OVERLAP";
        case BTD_GPT_SYSTEM_INDEX:return "G_SYSTEM_INDEX";
        case BTD_GPT_SYSTEM_ALIGNMENT:return "G_SYSTEM_ALIGN";
        case BTD_GPT_DUPLICATE_SYSTEM:return "G_DUP_SYSTEM";
        case BTD_GPT_SYSTEM_MISSING:return "G_SYSTEM_MISSING";
        default:return "G_UNKNOWN";
    }
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
