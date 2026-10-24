/* SPDX-License-Identifier: GPL-2.0-only. Synthetic, authenticated-format fixtures only. */
#include "bt_save_reader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks,diagnostics,skips;
static uint8_t *bytes;static size_t length;
#define CHECK(e) do{checks++;assert(e);}while(0)
void btd_status(const char *label,int result){CHECK(label!=NULL);if(result>0)diagnostics++;}
void btd_number(const char *label,uint32_t value){CHECK(label!=NULL);(void)value;}
void btd_skip(const char *label){CHECK(label!=NULL);skips++;}
static bool get_size(void *ctx,uint64_t *size){(void)ctx;*size=length;return true;}
static bool read_exact(void *ctx,uint64_t offset,void *out,size_t size) {
    (void)ctx;CHECK(offset<=length&&size<=length-offset);memcpy(out,bytes+(size_t)offset,size);return true;
}
static bool auth(void *ctx,const uint8_t *h,size_t n){(void)ctx;return n==0x4000&&!memcmp(h,"BSR-TEST-CMAC-v1",16);}
static void check(const char *name,BsrResult expected,const char *stage) {
    char path[120];snprintf(path,sizeof(path),"tests/corpus/%s.bin",name);
    FILE *f=fopen(path,"rb");CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);
    long n=ftell(f);CHECK(n>0&&n<1024*1024);length=(size_t)n;bytes=malloc(length);CHECK(bytes!=NULL);
    rewind(f);CHECK(fread(bytes,1,length,f)==length&&fclose(f)==0);
    BsrStorage storage={NULL,read_exact,get_size};BsrReader *reader=NULL;BsrError error={0};
    BsrResult result=bsr_open(&storage,auth,NULL,&reader,&error);
    CHECK(result==expected);
    if(expected==BSR_OK){uint64_t size;CHECK(reader&&bsr_file_size(reader,&size)==BSR_OK);}
    else {CHECK(!reader&&error.stage&&strcmp(error.stage,stage)==0);}
    bsr_close(&reader);free(bytes);bytes=NULL;
}
int main(void) {
#ifdef BTD_LEGACY_FS
    check("valid-fat-padding",BSR_FORMAT,"FAT geometry");
    check("valid-save-shrunk",BSR_FORMAT,"FAT geometry");
    check("valid-empty",BSR_RANGE,"FAT initial block");
    check("empty-invalid-marker",BSR_OK,NULL);
    printf("Legacy FS regression: %u checks; exact capacity, historical count and empty marker bugs reproduced\n",checks);
#else
    check("valid-fat-padding",BSR_OK,NULL);check("valid-save-shrunk",BSR_OK,NULL);check("valid-empty",BSR_OK,NULL);
    check("empty-invalid-marker",BSR_RANGE,"FAT initial block");
    check("fat-capacity-small",BSR_FORMAT,"FS_FAT_CAPACITY");check("fat-capacity-zero",BSR_FORMAT,"FS_FAT_CAPACITY");
    check("fs-block-zero",BSR_FORMAT,"FS_BLOCK_POWER");check("fs-block-not-power",BSR_FORMAT,"FS_BLOCK_POWER");
    check("fs-block-large",BSR_FORMAT,"FS_BLOCK_POWER");check("fs-block-mismatch",BSR_FORMAT,"FS_BLOCK_MATCH");
    check("fs-data-zero",BSR_FORMAT,"FS_DATA_COUNT");check("fs-data-large",BSR_FORMAT,"FS_DATA_COUNT");
    check("fs-data-range",BSR_FORMAT,"FS_DATA_RANGE");check("fat-capacity-storage",BSR_RANGE,"FS_FAT_STORAGE");
    check("fat-offset-unsupported",BSR_UNSUPPORTED,"FS_CONTROL_OFFSETS");
    CHECK(diagnostics>0&&skips>0);
    printf("FS geometry: %u checks passed (correct capacities, resized count, empty marker, bounds and precise stages)\n",checks);
#endif
    return 0;
}
