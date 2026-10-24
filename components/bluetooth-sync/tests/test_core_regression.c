#include "internal.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned cases;
static unsigned trace_failures;
void btd_status(const char *label,int result){assert(label);if(result>0)trace_failures++;}
void btd_number(const char *label,uint32_t value){assert(label);(void)value;}
void btd_skip(const char *label){assert(label);}
static long fail_malloc=-1,malloc_calls;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    if(fail_malloc>=0 && malloc_calls++==fail_malloc)return NULL;
    return __real_malloc(size);
}
typedef struct {
    uint8_t *bytes;size_t actual;uint64_t reported;long calls,fail_at;
    bool partial,get_size_fail;
    unsigned cmac_calls;
} Memory;
static bool memory_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Memory *memory=context;
    /* Assert that the parser never asks the backend for out-of-bounds bytes. */
    assert(bsr_range(offset,size,memory->reported));
    assert(bsr_range(offset,size,memory->actual));
    const long call=memory->calls++;
    if(call==memory->fail_at) {
        if(memory->partial && size!=0)memcpy(buffer,memory->bytes+(size_t)offset,size/2);
        return false;
    }
    memcpy(buffer,memory->bytes+(size_t)offset,size);return true;
}
static bool memory_size(void *context,uint64_t *size) {
    Memory *memory=context;
    if(memory->get_size_fail)return false;
    *size=memory->reported;return true;
}
static BsrResult memory_view_read(void *context,uint64_t offset,void *buffer,size_t size) {
    return memory_read(context,offset,buffer,size)?BSR_OK:BSR_IO;
}
static bool test_cmac(void *context,const uint8_t *header,size_t size) {
    Memory *memory=context;++memory->cmac_calls;
    /* Deliberately NONCRYPTOGRAPHIC synthetic hook. Never a production validator. */
    static const char tag[]="BSR-TEST-CMAC-v1";
    return size==0x4000 && memcmp(header,tag,16)==0;
}
static uint8_t *load(const char *name,size_t *size) {
    char path[160];const int n=snprintf(path,sizeof(path),"tests/corpus/%s.bin",name);
    assert(n>0 && (size_t)n<sizeof(path));
    FILE *file=fopen(path,"rb");assert(file!=NULL);
    assert(fseek(file,0,SEEK_END)==0);
    const long length=ftell(file);assert(length>0 && length<1024*1024);
    assert(fseek(file,0,SEEK_SET)==0);
    uint8_t *bytes=malloc((size_t)length);assert(bytes!=NULL);
    assert(fread(bytes,1,(size_t)length,file)==(size_t)length);
    assert(fclose(file)==0);*size=(size_t)length;return bytes;
}
static Memory fixture(const char *name) {
    Memory memory={.fail_at=-1};memory.bytes=load(name,&memory.actual);memory.reported=memory.actual;
    return memory;
}
static BsrResult open_memory(Memory *memory,BsrReader **out,BsrError *error) {
    const BsrStorage storage={memory,memory_read,memory_size};
    return bsr_open(&storage,test_cmac,memory,out,error);
}
static void dispose(Memory *memory) {bsr_wipe(memory->bytes,memory->actual);free(memory->bytes);}
static void check_payload(BsrReader *reader,bool empty) {
    uint64_t size=UINT64_MAX;assert(bsr_file_size(reader,&size)==BSR_OK);
    assert(size==(empty?0:700));uint8_t bytes[800];memset(bytes,0xA5,sizeof(bytes));
    assert(bsr_file_read(reader,0,bytes,(size_t)size)==BSR_OK);
    for(size_t i=0;i<(size_t)size;++i)assert(bytes[i]==(uint8_t)(i*17+3));
    if(!empty) {
        assert(bsr_file_read(reader,11,bytes,537)==BSR_OK);
        for(size_t i=0;i<537;++i)assert(bytes[i]==(uint8_t)((i+11)*17+3));
    }
    assert(bsr_file_read(reader,size,NULL,0)==BSR_OK);
    memset(bytes,0xA5,8);assert(bsr_file_read(reader,size,bytes,8)==BSR_RANGE);
    for(size_t i=0;i<8;++i)assert(bytes[i]==0);
    assert(bsr_file_read(reader,UINT64_MAX,bytes,8)==BSR_RANGE);
    bsr_wipe(bytes,sizeof(bytes));
}
static void corpus(void) {
    FILE *manifest=fopen("tests/corpus/cases.txt","rb");assert(manifest!=NULL);
    char expected[8],name[100];
    while(fscanf(manifest,"%7s %99s",expected,name)==2) {
        Memory memory=fixture(name);BsrReader *reader=NULL;BsrError error;
        const BsrResult result=open_memory(&memory,&reader,&error);
        const bool pass=memcmp(expected,"PASS",5)==0;
        if((result==BSR_OK)!=pass) {
            fprintf(stderr,"FAIL %s: result=%s stage=%s\n",name,bsr_result_string(result),error.stage);
            exit(2);
        }
        if(pass)check_payload(reader,strcmp(name,"valid-empty")==0);
        else assert(reader==NULL && error.result==result);
        if(strncmp(name,"cmac",4)==0 || strcmp(name,"header-b-cmac")==0) {
            assert(result==BSR_AUTH && memory.calls==2);
        }
        if(strcmp(name,"header-ab-disagree")==0)assert(result==BSR_AMBIGUOUS && memory.calls==2);
        printf("%s: %s (%s)\n",expected,name,pass?"all layers + /file":error.stage);
        bsr_close(&reader);assert(reader==NULL);bsr_close(&reader);dispose(&memory);++cases;
    }
    assert(fclose(manifest)==0);
}
static void crypto_vectors(void) {
    static const uint8_t abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,
        0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static const uint8_t empty[32]={0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,
        0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,0x27,0xae,0x41,0xe4,
        0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55};
    uint8_t digest[32];bsr_sha256("abc",3,digest);assert(memcmp(digest,abc,32)==0);
    bsr_sha256("",0,digest);assert(memcmp(digest,empty,32)==0);bsr_wipe(digest,32);cases+=2;
    /* The independently built corpus exercises 512-byte padded hashing + HMAC salts. */
}
static void failure_injection(void) {
    Memory baseline=fixture("valid-v5");BsrReader *reader=NULL;BsrError error;
    fail_malloc=LONG_MAX;malloc_calls=0;
    assert(open_memory(&baseline,&reader,&error)==BSR_OK);
    const long allocations=malloc_calls,reads=baseline.calls;
    fail_malloc=-1;bsr_close(&reader);
    for(long i=0;i<allocations;++i) {
        Memory memory=fixture("valid-v5");fail_malloc=i;malloc_calls=0;
        const BsrResult result=open_memory(&memory,&reader,&error);fail_malloc=-1;
        assert(result==BSR_MEMORY && reader==NULL);dispose(&memory);++cases;
    }
    for(long i=0;i<reads;++i)for(unsigned partial=0;partial<2;++partial) {
        Memory memory=fixture("valid-v5");memory.fail_at=i;memory.partial=partial!=0;
        const BsrResult result=open_memory(&memory,&reader,&error);
        assert(result==BSR_IO && reader==NULL && memory.calls==i+1);dispose(&memory);++cases;
    }
    printf("Injected clean errors at %ld allocation sites and %ld read sites (full/partial).\n",allocations,reads);
    baseline.calls=0;baseline.cmac_calls=0;baseline.get_size_fail=true;
    assert(open_memory(&baseline,&reader,&error)==BSR_IO && baseline.calls==0);++cases;
    baseline.get_size_fail=false;
    const uint64_t sizes[]={0,1,0x3FFF,0x4000,0x7FFF,0x8000,0x8001,0xD3FF};
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        baseline.calls=0;baseline.reported=sizes[i];
        assert(open_memory(&baseline,&reader,&error)!=BSR_OK && reader==NULL);++cases;
    }
    baseline.reported=baseline.actual;baseline.calls=0;
    const BsrStorage storage={&baseline,memory_read,memory_size};
    assert(bsr_open(&storage,NULL,NULL,&reader,&error)==BSR_ARGUMENT && baseline.calls==0);++cases;
    assert(open_memory(&baseline,&reader,&error)==BSR_OK);
    baseline.fail_at=baseline.calls;baseline.partial=true;
    uint8_t output[700];memset(output,0xA5,sizeof(output));
    assert(bsr_file_read(reader,0,output,sizeof(output))==BSR_IO);
    for(size_t i=0;i<sizeof(output);++i)assert(output[i]==0);
    assert(bsr_file_read(reader,0,output,1)==BSR_FORMAT);++cases;
    bsr_close(&reader);dispose(&baseline);
}
static void units(void) {
    uint64_t result=0;
    assert(!bsr_mul(UINT64_MAX,2,&result));assert(bsr_mul(UINT64_MAX,1,&result));
    assert(!bsr_range(UINT64_MAX-1,4,UINT64_MAX));assert(bsr_range(0,0,0));cases+=4;
    uint8_t a[64],b[64],out[40],bitmap[4]={0,0,0,0x40};
    memset(a,0x11,sizeof(a));memset(b,0x22,sizeof(b));
    Bytes ab={a,sizeof(a)},bb={b,sizeof(b)};Duplex duplex;
    assert(bsr_duplex_init(&duplex,bsr_bytes(&ab),bsr_bytes(&bb),bitmap,4,5)==BSR_OK);
    assert(bsr_read(duplex.view,24,out,40)==BSR_OK);
    for(size_t i=0;i<40;++i)assert(out[i]==(i<8?0x11:0x22));
    ++cases;
    assert(bsr_duplex_init(&duplex,bsr_bytes(&ab),bsr_bytes(&bb),bitmap,3,5)==BSR_RANGE);++cases;
    assert(bsr_duplex_init(&duplex,bsr_bytes(&ab),bsr_bytes(&bb),bitmap,4,64)==BSR_FORMAT);++cases;
    BsrReader *owner=malloc(sizeof(*owner));assert(owner!=NULL);memset(owner,0,sizeof(*owner));
    uint8_t raw_header[64]={0};Remap map;
    assert(bsr_remap_init(owner,&map,raw_header,0,0,(View){0})==BSR_AUTH);++cases;
    owner->error=(BsrError){0};owner->memory_used=sizeof(*owner);
    assert(bsr_alloc(owner,0,8)==NULL);++cases;
    owner->error=(BsrError){0};assert(bsr_alloc(owner,UINT64_MAX,8)==NULL);++cases;
    owner->error=(BsrError){0};assert(bsr_alloc(owner,BSR_MEMORY_BUDGET,1)==NULL);++cases;
    bsr_close(&owner);
    /* White-box tests only: private validated Remap with two widely separated segments. */
    uint8_t table[64]={0},backing[0x8400]={0};
    const uint64_t high_segment=UINT64_C(1)<<63;
    for(unsigned i=0;i<2;++i) {
        const uint64_t virtual_offset=i==0?0:high_segment;
        const uint64_t physical=(uint64_t)i*0x200;
        for(unsigned j=0;j<8;++j) {
            table[i*32+j]=(uint8_t)(virtual_offset>>(j*8));
            table[i*32+8+j]=(uint8_t)(physical>>(j*8));
        }
        table[i*32+17]=2;table[i*32+25]=2;
    }
    memcpy(backing+0x8000,table,64);memset(backing,0x31,512);memset(backing+512,0x42,512);
    Memory unit_memory={.bytes=backing,.actual=1024,.reported=1024,.fail_at=-1};
    owner=malloc(sizeof(*owner));assert(owner!=NULL);memset(owner,0,sizeof(*owner));
    owner->authenticated=true;owner->memory_used=sizeof(*owner);
    Bytes backing_bytes={backing,sizeof(backing)};owner->raw=bsr_bytes(&backing_bytes);
    uint8_t map_header[64]={'R','M','A','P',0,0,1,0,2,0,0,0,2,0,0,0,1};
    Remap unit_map;
    assert(bsr_remap_init(owner,&unit_map,map_header,0x8000,64,
                         (View){&unit_memory,memory_view_read,1024})==BSR_OK);
    assert(bsr_read(unit_map.view,high_segment,out,40)==BSR_OK);
    for(size_t i=0;i<40;++i)assert(out[i]==0x42);
    unit_memory.calls=0;
    assert(bsr_read(unit_map.view,500,out,40)==BSR_RANGE && unit_memory.calls==0);
    cases+=2;bsr_close(&owner);
}
static uint32_t random_word(uint32_t *seed) {
    *seed^=*seed<<13;*seed^=*seed>>17;*seed^=*seed<<5;return *seed;
}
static void mutations(void) {
    Memory base=fixture("valid-v5");uint32_t seed=0xC0FFEE;
    for(unsigned iteration=0;iteration<2000;++iteration) {
        Memory memory=fixture("valid-v5");
        const size_t offset=random_word(&seed)%memory.actual;
        memory.bytes[offset]^=(uint8_t)(1u<<(random_word(&seed)%8));
        if(offset<0x4000) {
            bsr_sha256(memory.bytes+0x300,0x3D00,memory.bytes+0x108);
            memcpy(memory.bytes+0x4000,memory.bytes,0x4000);
        }
        BsrReader *reader=NULL;BsrError error;
        const BsrResult result=open_memory(&memory,&reader,&error);
        if(result==BSR_OK)check_payload(reader,false);else assert(reader==NULL);
        bsr_close(&reader);dispose(&memory);++cases;
    }
    /* Mutation after open is also verified, and failed read clears entire caller buffer. */
    BsrReader *reader=NULL;BsrError error;assert(open_memory(&base,&reader,&error)==BSR_OK);
    /* Locate a genuine backing byte through the journal and remap, not an invented FS offset. */
    const uint64_t virtual_position=0x4400+(uint64_t)reader->journal.map[2]*512+6;
    const MapEntry *entry=NULL;
    for(uint32_t i=0;i<reader->main_map.count;++i) {
        const MapEntry *candidate=&reader->main_map.entries[i];
        if(virtual_position>=candidate->virtual_offset && virtual_position-candidate->virtual_offset<candidate->size)
            entry=candidate;
    }
    assert(entry!=NULL);
    const uint64_t physical=reader->main_slice.offset+entry->physical_offset+virtual_position-entry->virtual_offset;
    assert(physical<base.actual);base.bytes[physical]^=1;
    uint8_t output[700];memset(output,0xA5,sizeof(output));
    assert(bsr_file_read(reader,0,output,sizeof(output))==BSR_INTEGRITY);
    for(size_t i=0;i<sizeof(output);++i)assert(output[i]==0);
    ++cases;
    bsr_close(&reader);dispose(&base);
}
int main(void) {
    assert(setvbuf(stdout,NULL,_IOLBF,0)==0);
    crypto_vectors();units();corpus();failure_injection();mutations();
    printf("PASS: %u cases; ASan/UBSan; synthetic corpus only; no Switch execution.\n",cases);
    assert(trace_failures>0);printf("IVFC diagnostics observed %u synthetic failures without bypassing checks.\n",trace_failures);
    return 0;
}
