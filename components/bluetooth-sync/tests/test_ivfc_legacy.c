/* SPDX-License-Identifier: GPL-2.0-only. Prove the old code fails a correct synthetic save. */
#include "bt_save_reader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *bytes;static size_t length;
static bool size(void *ctx,uint64_t *out){(void)ctx;*out=length;return true;}
static bool read_exact(void *ctx,uint64_t off,void *out,size_t n){(void)ctx;if(off>length||n>length-off)return false;memcpy(out,bytes+off,n);return true;}
static bool auth(void *ctx,const uint8_t *h,size_t n){(void)ctx;return n==0x4000&&!memcmp(h,"BSR-TEST-CMAC-v1",16);}
int main(void) {
    FILE *f=fopen("tests/corpus/valid-v5.bin","rb");assert(f);
    assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>0&&n<1024*1024);length=(size_t)n;
    bytes=malloc(length);assert(bytes);rewind(f);assert(fread(bytes,1,length,f)==length&&fclose(f)==0);
    BsrStorage s={NULL,read_exact,size};BsrReader *reader=NULL;BsrError error={0};
    BsrResult result=bsr_open(&s,auth,NULL,&reader,&error);
    assert(result==BSR_INTEGRITY&&!reader&&error.stage&&!strcmp(error.stage,"data IVFC verification"));
    free(bytes);puts("Legacy regression: expected data IVFC verification ERROR (6) reproduced on correct synthetic save");return 0;
}
