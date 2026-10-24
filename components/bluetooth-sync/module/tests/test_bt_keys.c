/* SPDX-License-Identifier: GPL-2.0-only. All fixtures are synthetic. */
#include "bt_keys.h"
#include "bt_only.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
#define BIS "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
#define MAC "202122232425262728292a2b2c2d2e2f"
#define Z16 "00000000000000000000000000000000"
#define LINE_BIS "bis_key_02 = " BIS "\n"
#define LINE_MAC "save_mac_key = " MAC "\n"
typedef struct { const uint8_t *data;size_t size,position;bool fail; } Input;
static bool read_input(void *ctx,void *out,size_t n) {
    Input *p=ctx;CHECK(n<=128&&n<=p->size-p->position);
    if(p->fail){memset(out,0xA5,n);return false;}
    memcpy(out,p->data+p->position,n);p->position+=n;return true;
}
static bool zeroed(const void *p,size_t n) {
    const uint8_t *b=p;while(n--)if(*b++)return false;return true;
}
static BtoKeyResult parse(const void *data,size_t size,BtoKeys *keys,bool fail) {
    Input input={data,size,0,fail};memset(keys,0xA5,sizeof(*keys));
    BtoKeyResult result=bto_keys_parse(read_input,&input,size,keys);
    if(result!=BTO_KEYS_OK)CHECK(zeroed(keys,sizeof(*keys)));
    return result;
}
static void expect(const char *s,BtoKeyResult expected) {
    BtoKeys keys;CHECK(parse(s,strlen(s),&keys,false)==expected);
    if(expected==BTO_KEYS_OK) {
        for(unsigned i=0;i<32;i++)CHECK(keys.bis_system[i]==i);
        for(unsigned i=0;i<16;i++)CHECK(keys.save_mac[i]==32+i);
    }
    bto_wipe(&keys,sizeof(keys));
}
static bool read_stdin(void *ctx,void *out,size_t n) {
    (void)ctx;return fread(out,1,n,stdin)==n;
}
int main(int argc,char **argv) {
    if(argc==3&&strcmp(argv[1],"--stdin-size")==0) {
        char *end=NULL;unsigned long size=strtoul(argv[2],&end,10);
        if(!end||*end||!size||size>BTO_KEYS_FILE_MAX)return 2;
        BtoKeys keys={0};BtoKeyResult result=bto_keys_parse(read_stdin,NULL,(size_t)size,&keys);
        bto_wipe(&keys,sizeof(keys));
        printf("minimal key parser: %s; values never printed or persisted\n",result==BTO_KEYS_OK?"two required keys accepted":"rejected");
        return result==BTO_KEYS_OK?0:1;
    }
    BtoKeys keys;
    expect(LINE_BIS LINE_MAC,BTO_KEYS_OK);
    expect("\xef\xbb\xbf\t" LINE_MAC "  bis_key_02\t=\t" BIS " # comment\r\n",BTO_KEYS_OK);
    expect("# comment\r\nmaster_key_00 = invalid-ignored\r\n" LINE_BIS "save_mac_key=" MAC,BTO_KEYS_OK);
    expect(LINE_BIS,BTO_KEYS_MISSING);expect(LINE_MAC,BTO_KEYS_MISSING);
    expect(LINE_BIS LINE_MAC LINE_BIS,BTO_KEYS_DUPLICATE);
    expect(LINE_BIS LINE_MAC LINE_MAC,BTO_KEYS_DUPLICATE);
    expect("bis_key_02 = g" BIS "\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 = 0011\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 = " BIS "0\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 " BIS "\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 = " Z16 Z16 "\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 = " Z16 MAC "\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect("bis_key_02 = " MAC Z16 "\n" LINE_MAC,BTO_KEYS_FORMAT);
    expect(LINE_BIS "save_mac_key = " Z16 "\n",BTO_KEYS_FORMAT);
    expect(LINE_BIS "save_mac_key = " MAC " extra\n",BTO_KEYS_FORMAT);
    expect(LINE_BIS "save_mac_key = " MAC " ; comment\n",BTO_KEYS_OK);
    CHECK(parse(LINE_BIS LINE_MAC,sizeof(LINE_BIS LINE_MAC)-1,&keys,true)==BTO_KEYS_IO);
    CHECK(parse("",0,&keys,false)==BTO_KEYS_ARGUMENT);
    CHECK(parse("",BTO_KEYS_FILE_MAX+1,&keys,false)==BTO_KEYS_ARGUMENT);
    CHECK(bto_keys_parse(NULL,NULL,5,&keys)==BTO_KEYS_ARGUMENT&&zeroed(&keys,sizeof(keys)));
    CHECK(bto_keys_parse(read_input,NULL,5,NULL)==BTO_KEYS_ARGUMENT);
    char long_line[4096];memset(long_line,'x',sizeof(long_line));
    memcpy(long_line,"unrelated_key = ",16);long_line[2048]='\n';
    memcpy(long_line+2049,LINE_BIS LINE_MAC,sizeof(LINE_BIS LINE_MAC));
    expect(long_line,BTO_KEYS_OK);
    memcpy(long_line,"bis_key_02 = ",13);expect(long_line,BTO_KEYS_FORMAT);
    memset(long_line,' ',2048);memcpy(long_line+2048,LINE_BIS LINE_MAC,sizeof(LINE_BIS LINE_MAC));
    expect(long_line,BTO_KEYS_OK);
    memcpy(long_line+2048,LINE_BIS LINE_MAC LINE_BIS,sizeof(LINE_BIS LINE_MAC LINE_BIS));
    expect(long_line,BTO_KEYS_DUPLICATE);
    char nul_line[]=LINE_BIS LINE_MAC; nul_line[4]=0;
    CHECK(parse(nul_line,sizeof(nul_line)-1,&keys,false)==BTO_KEYS_FORMAT);
    /* Every truncation of a valid file must fail until both key values exist. */
    const char *complete=LINE_BIS LINE_MAC;
    for(size_t i=1;i<strlen(complete)-1;i++)CHECK(parse(complete,i,&keys,false)!=BTO_KEYS_OK);
    /* Vary chunk boundaries using unrelated line lengths. */
    for(unsigned prefix=0;prefix<512;prefix++) {
        memset(long_line,'x',prefix);long_line[prefix]='\n';
        memcpy(long_line+prefix+1,complete,strlen(complete)+1);expect(long_line,BTO_KEYS_OK);
    }
    puts("minimal key parser: strict format, duplicates, bounds, IO failures and memory clearing passed (synthetic)");
    printf("minimal key parser: %u checks passed\n",checks);return 0;
}
