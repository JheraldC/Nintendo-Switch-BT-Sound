/* SPDX-License-Identifier: GPL-2.0-only */
#include "bt_probe.h"
#include "bt_only.h"
#include <string.h>
/* Logical /file offsets, NEVER offsets into the raw save container.
 * Sources and limitations: BLUETOOTH-FORMAT.md. Unknown profiles stop before BT.
 * The 24 allocated slots are split 10 + 14; they are not a contiguous table.
 */
enum { SETTINGS_SIZE=0x80000, SETTINGS_VERSION=0x140000,
       COUNT_OFFSET=0x8060, FIRST_OFFSET=0x8070, LAST_OFFSET=0x2AAA0,
       FIRST_SLOTS=10, TOTAL_SLOTS=24 };
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
/* The sole caller of bsr_file_read in the payload. Explicit shape allowlist. */
static __attribute__((noinline)) BsrResult settings_read(BsrReader *reader,
    uint64_t offset,void *buffer,size_t size,BtdBtSummary *summary) {
    bool allowed=(offset==0&&size==16)||(offset==COUNT_OFFSET&&size==4);
    if(size==BTO_RECORD_SIZE) {
        allowed=(offset>=FIRST_OFFSET&&offset<FIRST_OFFSET+FIRST_SLOTS*BTO_RECORD_SIZE
                 &&(offset-FIRST_OFFSET)%BTO_RECORD_SIZE==0)
             ||(offset>=LAST_OFFSET&&offset<LAST_OFFSET+(TOTAL_SLOTS-FIRST_SLOTS)*BTO_RECORD_SIZE
                 &&(offset-LAST_OFFSET)%BTO_RECORD_SIZE==0);
    }
    if(!allowed||summary->bytes_read>20+TOTAL_SLOTS*BTO_RECORD_SIZE-size)return BSR_RANGE;
    BsrResult result=bsr_file_read(reader,offset,buffer,size);
    if(result==BSR_OK)summary->bytes_read+=(uint32_t)size;
    return result;
}
BsrResult btd_v6_bt_probe(BsrReader *reader,BtdBtSummary *out) {
    if(out==NULL)return BSR_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    if(reader==NULL)return BSR_ARGUMENT;
    uint8_t header[16]={0},count_bytes[4]={0},seen[TOTAL_SLOTS][6]={{0}};
    BtoRecord record={{0}}; BtoNintendoType type=BTO_NINTENDO_NONE;
    uint32_t totals[4]={0}; uint64_t size=0; BsrResult result;
    out->stage=BTD_BT_SIZE;
    result=bsr_file_size(reader,&size);if(result!=BSR_OK)goto finish;
    if(size!=SETTINGS_SIZE){result=BSR_UNSUPPORTED;goto finish;}
    out->stage=BTD_BT_HEADER;
    result=settings_read(reader,0,header,sizeof(header),out);if(result!=BSR_OK)goto finish;
    out->version=le32(header);out->flags=le32(header+4);
    out->stage=BTD_BT_PROFILE;
    if(out->version!=SETTINGS_VERSION||out->flags!=7){result=BSR_UNSUPPORTED;goto finish;}
    for(size_t i=8;i<sizeof(header);i++)if(header[i]!=0){result=BSR_UNSUPPORTED;goto finish;}
    out->layout_known=1;out->stage=BTD_BT_COUNT;
    result=settings_read(reader,COUNT_OFFSET,count_bytes,sizeof(count_bytes),out);if(result!=BSR_OK)goto finish;
    out->count=le32(count_bytes);
    if(out->count>TOTAL_SLOTS){result=BSR_LIMIT;goto finish;}
    for(uint32_t i=0;i<out->count;i++) {
        out->failed_slot=i+1;out->stage=BTD_BT_READ;
        uint64_t offset=i<FIRST_SLOTS?FIRST_OFFSET+(uint64_t)i*BTO_RECORD_SIZE
            :LAST_OFFSET+(uint64_t)(i-FIRST_SLOTS)*BTO_RECORD_SIZE;
        result=settings_read(reader,offset,&record,sizeof(record),out);if(result!=BSR_OK)goto finish;
        out->stage=BTD_BT_RECORD;
        BtoResult classified=bto_nintendo_type(&record,&type);
        if(classified!=BTO_OK&&classified!=BTO_NOT_NINTENDO&&classified!=BTO_NO_LINK_KEY) {
            result=BSR_FORMAT;goto finish;
        }
        out->stage=BTD_BT_DUPLICATE;
        for(uint32_t j=0;j<i;j++)if(memcmp(seen[j],record.bytes,6)==0){result=BSR_AMBIGUOUS;goto finish;}
        memcpy(seen[i],record.bytes,6);
        out->stage=BTD_BT_KEY;
        if(classified==BTO_NO_LINK_KEY){result=BSR_AUTH;goto finish;}
        totals[classified==BTO_NOT_NINTENDO?0:(unsigned)type]++;
        out->validated++;
        bto_wipe(&record,sizeof(record));type=BTO_NINTENDO_NONE;
    }
    out->other=totals[0];out->joy_l=totals[1];out->joy_r=totals[2];out->pro=totals[3];
    out->stage=BTD_BT_DONE;out->failed_slot=0;out->complete=1;result=BSR_OK;
finish:
    /* Partial success never becomes an actionable set of pairings. */
    if(result!=BSR_OK)out->validated=out->joy_l=out->joy_r=out->pro=out->other=out->complete=0;
    bto_wipe(header,sizeof(header));bto_wipe(count_bytes,sizeof(count_bytes));
    bto_wipe(seen,sizeof(seen));bto_wipe(&record,sizeof(record));bto_wipe(totals,sizeof(totals));
    return result;
}
