#include "bt_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,calls,fail;
static uint8_t file[0x80000];
#define CHECK(e) do{checks++;assert(e);}while(0)
static void put(size_t off,uint32_t v){for(size_t i=0;i<4;i++)file[off+i]=(uint8_t)(v>>(8*i));}
static size_t slot(unsigned i){return i<10?0x8070+i*512:0x2AAA0+(i-10)*512;}
BsrResult bsr_file_size(const BsrReader *r,uint64_t *out){(void)r;*out=sizeof(file);return BSR_OK;}
BsrResult bsr_file_read(BsrReader *r,uint64_t off,void *out,size_t n){
    (void)r;calls++;if(fail==calls)return BSR_IO;
    CHECK((off==0&&n==16)||(off==0x8060&&n==4)||(n==512&&((off>=0x8070&&off<0x9470)||(off>=0x2AAA0&&off<0x2C6A0))));
    memcpy(out,file+off,n);return BSR_OK;
}
int main(void){
    BsrReader *r=(BsrReader *)(void *)file;BtdBtSummary summary;BthRecords records;
    for(unsigned count=0;count<=24;count++){
        memset(file,0xA5,sizeof(file));memset(file,0,16);put(0,0x220000);put(4,7);put(0x8060,count);
        for(unsigned i=0;i<count;i++){
            uint8_t *p=file+slot(i);memset(p,0,512);p[0]=2;p[5]=(uint8_t)(i+1);p[0x29]=42;p[0x39]=1;
            strcpy((char *)p+0xD6,"SYNTHETIC GENERIC");
        }
        calls=fail=0;CHECK(btd_bt_collect(r,&summary,&records)==BSR_OK);CHECK(records.count==count&&calls==count+2);
        for(unsigned i=0;i<count;i++)CHECK(!memcmp(records.record[i],file+slot(i),512));
        for(unsigned failure=1;failure<=count+2;failure++){
            calls=0;fail=failure;CHECK(btd_bt_collect(r,&summary,&records)==BSR_IO);
            for(size_t j=0;j<sizeof(records);j++)CHECK(((uint8_t *)&records)[j]==0);
        }
    }
    printf("Selective collection: %u checks passed; split slots, only Bluetooth bytes, complete wipe on any read failure\n",checks);return 0;
}
