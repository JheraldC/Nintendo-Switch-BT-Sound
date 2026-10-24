/* SPDX-License-Identifier: GPL-2.0-only
 * Filesystem unit: dimensions from the photo, all data freshly synthetic.
 * Outer authentication is a precondition here; the full-reader corpus tests
 * exercise the authentication callback and real integrity layers separately.
 */
#include "internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { BLOCK=16384, BLOCKS=34, DATA_BYTES=BLOCK*BLOCKS, FAT_BYTES=16384 };
static uint8_t data[DATA_BYTES],fat[FAT_BYTES];
static Bytes data_bytes={data,sizeof(data)},fat_bytes={fat,sizeof(fat)};
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
void btd_status(const char *label,int code){CHECK(label!=NULL);(void)code;}
void btd_number(const char *label,uint32_t value){CHECK(label!=NULL);(void)value;}
void btd_skip(const char *label){CHECK(label!=NULL);}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
static void fixture(void) {
    memset(data,0,sizeof(data));memset(fat,0,sizeof(fat));
    put32(data,3);put32(data+4,3);put32(data+0xBC,2);put32(data+0xC0+0x4C,2);
    uint8_t *table=data+BLOCK;
    put32(table,3);put32(table+4,3);put32(table+0xBC,2);
    put32(table+0xC0,2);memcpy(table+0xC4,"file",5);
    put32(table+0xC0+0x48,2);put64(table+0xC0+0x4C,700);
    for(unsigned i=0;i<700;i++)data[2*BLOCK+i]=(uint8_t)(i*17+3);
    put32(fat+4,5); /* Free chain begins at data block 4, FAT entry 5. */
    put32(fat+1*8,0x80000000);put32(fat+2*8,0x80000000);
    put32(fat+3*8,0x80000000);put32(fat+3*8+4,0x80000000);
    put32(fat+4*8,0x80000003);put32(fat+4*8+4,4);
    put32(fat+5*8,0x80000000);put32(fat+5*8+4,0x80000000);
    put32(fat+6*8,0x80000005);put32(fat+6*8+4,34);memcpy(fat+34*8,fat+6*8,8);
}
static void check(uint32_t managed,uint64_t fat_size,uint64_t data_size,BsrResult expected,const char *stage) {
    fixture();BsrReader *r=calloc(1,sizeof(*r));CHECK(r!=NULL);r->memory_used=sizeof(*r);r->authenticated=true;
    uint8_t *h=r->header+0x608,*control=h+0x18;
    put32(r->header+0x104,0x50000);memcpy(h,"SAVE",4);put32(h+4,0x60000);put64(h+8,BLOCKS);put64(h+16,BLOCK);
    put64(control,BLOCK);put32(control+16,managed);put32(control+32,BLOCKS);
    put32(control+40,0);put32(control+44,1);
    r->data_ivfc.view=bsr_bytes(&data_bytes);r->data_ivfc.view.size=data_size;
    r->fat_ivfc.view=bsr_bytes(&fat_bytes);r->fat_ivfc.view.size=fat_size;
    uint8_t snapshot[BSR_HEADER_SIZE];memcpy(snapshot,r->header,sizeof(snapshot));
    BsrResult result=bsr_filesystem_init(r);CHECK(result==expected);CHECK(!memcmp(snapshot,r->header,sizeof(snapshot)));
    if(result==BSR_OK) {
        CHECK(r->fat_entries==35&&r->data_blocks==34&&r->block_size==16384&&r->target_size==700);
        uint8_t payload[700];CHECK(bsr_read(r->target_chain.view,0,payload,sizeof(payload))==BSR_OK);
        for(unsigned i=0;i<sizeof(payload);i++)CHECK(payload[i]==(uint8_t)(i*17+3));
    } else CHECK(r->error.stage&&strcmp(r->error.stage,stage)==0);
    bsr_close(&r);CHECK(r==NULL);
}
int main(void) {
#ifdef BTD_V4_REGRESSION
    check(34,FAT_BYTES,DATA_BYTES,BSR_FORMAT,"FS_FAT_CAPACITY");
    printf("V4 console-geometry regression: %u checks; photographed 34/34 shape reproduces ERROR (4)\n",checks);
#else
    check(34,FAT_BYTES,DATA_BYTES,BSR_OK,NULL);
    check(34,35*8,DATA_BYTES,BSR_OK,NULL); /* Reserved entry fits exactly. */
    check(35,FAT_BYTES,DATA_BYTES,BSR_OK,NULL);
    check(33,FAT_BYTES,DATA_BYTES,BSR_FORMAT,"FS_FAT_CAPACITY");
    check(34,34*8,DATA_BYTES,BSR_RANGE,"FS_FAT_STORAGE");
    check(34,35*8-1,DATA_BYTES,BSR_RANGE,"FS_FAT_STORAGE");
    check(UINT32_MAX,FAT_BYTES,DATA_BYTES,BSR_RANGE,"FS_FAT_STORAGE");
    check(34,FAT_BYTES,DATA_BYTES-1,BSR_FORMAT,"FS_DATA_RANGE");
    printf("Console geometry: %u checks passed (34 blocks, 16 KiB, reserved entry, truncation and overflow)\n",checks);
#endif
    return 0;
}
