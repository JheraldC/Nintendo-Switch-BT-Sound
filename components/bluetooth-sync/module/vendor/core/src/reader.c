#include "internal.h"
#include <stdlib.h>
#include <string.h>

static BsrResult raw_read(void *context,uint64_t offset,void *buffer,size_t size) {
    BsrReader *r=context;
    if(!bsr_range(offset,size,r->raw.size))return BSR_RANGE;
    return r->storage.ReadExact(r->storage.context,offset,buffer,size)?BSR_OK:BSR_IO;
}
static BsrResult headers(BsrReader *r,BsrValidateCmac validate,void *context) {
    if(!bsr_range(0,2*BSR_HEADER_SIZE,r->raw.size))return bsr_fail(r,BSR_RANGE,"header truncated");
    uint8_t *other=bsr_alloc(r,BSR_HEADER_SIZE,1);if(other==NULL)return r->error.result;
    BsrResult result=bsr_read(r->raw,0,r->header,BSR_HEADER_SIZE);
    if(result!=BSR_OK)return bsr_fail(r,result,"header A read");
    result=bsr_read(r->raw,BSR_HEADER_SIZE,other,BSR_HEADER_SIZE);
    if(result!=BSR_OK)return bsr_fail(r,result,"header B read");
    for(unsigned i=0;i<2;++i) {
        const uint8_t *header=i==0?r->header:other;
        if(!validate(context,header,BSR_HEADER_SIZE))return bsr_fail(r,BSR_AUTH,"header CMAC");
        if(bsr_u32(header+0x100)!=UINT32_C(0x46534944)
           || (bsr_u32(header+0x104)!=0x40000 && bsr_u32(header+0x104)!=0x50000))
            return bsr_fail(r,BSR_UNSUPPORTED,"DISF version");
        uint8_t digest[32];bsr_sha256(header+0x300,0x3D00,digest);
        const bool valid=memcmp(digest,header+0x108,32)==0;bsr_wipe(digest,32);
        if(!valid)return bsr_fail(r,BSR_INTEGRITY,"header SHA-256");
    }
    if(memcmp(r->header,other,BSR_HEADER_SIZE)!=0)
        return bsr_fail(r,BSR_AMBIGUOUS,"header A/B disagree");
    bsr_wipe(other,BSR_HEADER_SIZE);r->authenticated=true;
    r->header_bytes=(Bytes){r->header,BSR_HEADER_SIZE};r->header_view=bsr_bytes(&r->header_bytes);
    return BSR_OK;
}
static bool overlap(uint64_t a,uint64_t an,uint64_t b,uint64_t bn) {
    /* All ranges were already checked against the same backing storage. */
    return a<b+bn && b<a+an;
}
static BsrResult initialize_maps_and_duplex(BsrReader *r) {
    const uint8_t *layout=r->header+0x100,*dpfs=r->header+0x300;
    const uint64_t table_a=bsr_u64(layout+0x28),table_a_size=bsr_u64(layout+0x30);
    const uint64_t table_b=bsr_u64(layout+0x38),table_b_size=bsr_u64(layout+0x40);
    const uint64_t data=bsr_u64(layout+0x48),data_size=bsr_u64(layout+0x50);
    if(table_a<0x8000 || table_b<0x8000 || data<0x8000 || table_a_size==0 || table_b_size==0 || data_size==0
       || !bsr_range(table_a,table_a_size,r->raw.size) || !bsr_range(table_b,table_b_size,r->raw.size)
       || !bsr_range(data,data_size,r->raw.size))return bsr_fail(r,BSR_RANGE,"top-level geometry");
    if(overlap(table_a,table_a_size,table_b,table_b_size)
       || overlap(table_a,table_a_size,data,data_size) || overlap(table_b,table_b_size,data,data_size))
        return bsr_fail(r,BSR_FORMAT,"top-level overlap");
    View main_base;BsrResult result=bsr_slice(&r->main_slice,r->raw,data,data_size,&main_base);
    if(result!=BSR_OK)return bsr_fail(r,result,"main storage");
    result=bsr_remap_init(r,&r->main_map,r->header+0x650,table_a,table_a_size,main_base);
    if(result!=BSR_OK)return result;
    if(bsr_u32(dpfs)!=UINT32_C(0x53465044) || bsr_u32(dpfs+4)!=0x10000)
        return bsr_fail(r,BSR_UNSUPPORTED,"DPFS header");
    const uint64_t bitmap_size=bsr_u64(layout+0x68),metadata_size=bsr_u64(layout+0x80);
    const uint64_t master_size=bsr_u64(layout+0xB8),master_a=bsr_u64(layout+0xA8),master_b=bsr_u64(layout+0xB0);
    const uint64_t additional_start=bsr_u32(layout+4)==0x50000?0xBA0:0xAD8;
    if(r->header[0x258]>1 || master_size==0 || master_a<additional_start || master_b<additional_start
       || !bsr_range(master_a,master_size,BSR_HEADER_SIZE) || !bsr_range(master_b,master_size,BSR_HEADER_SIZE)
       || bitmap_size==0 || metadata_size==0 || bitmap_size>BSR_MEMORY_BUDGET || metadata_size>BSR_MEMORY_BUDGET
       || bitmap_size!=bsr_u64(dpfs+0x24) || metadata_size!=bsr_u64(dpfs+0x38)
       || bsr_u32(dpfs+0x18)>30)return bsr_fail(r,BSR_FORMAT,"duplex geometry");
    const uint64_t offsets[4]={bsr_u64(layout+0x58),bsr_u64(layout+0x60),
                               bsr_u64(layout+0x70),bsr_u64(layout+0x78)};
    const uint64_t sizes[4]={bitmap_size,bitmap_size,metadata_size,metadata_size};
    for(unsigned i=0;i<4;++i) {
        if(!bsr_range(offsets[i],sizes[i],r->main_map.view.size))return bsr_fail(r,BSR_RANGE,"duplex backing range");
        for(unsigned j=0;j<i;++j)if(overlap(offsets[i],sizes[i],offsets[j],sizes[j]))
            return bsr_fail(r,BSR_FORMAT,"duplex backing overlap");
    }
    Slice *slices=bsr_alloc(r,4,sizeof(*slices));if(slices==NULL)return r->error.result;
    View views[4];
    for(unsigned i=0;i<4;++i) {
        result=bsr_slice(&slices[i],r->main_map.view,offsets[i],sizes[i],&views[i]);
        if(result!=BSR_OK)return bsr_fail(r,result,"duplex slice");
    }
    const uint64_t active=r->header[0x258]==0?master_a:master_b;
    result=bsr_duplex_init(&r->bitmap_duplex,views[0],views[1],r->header+(size_t)active,
                           (size_t)master_size,bsr_u32(dpfs+0x2C));
    if(result!=BSR_OK)return bsr_fail(r,result,"duplex bitmap");
    uint8_t *bitmap=bsr_alloc(r,bitmap_size,1);if(bitmap==NULL)return r->error.result;
    result=bsr_read(r->bitmap_duplex.view,0,bitmap,(size_t)bitmap_size);
    if(result!=BSR_OK)return bsr_fail(r,result,"duplex bitmap read");
    result=bsr_duplex_init(&r->metadata_duplex,views[2],views[3],bitmap,(size_t)bitmap_size,bsr_u32(dpfs+0x40));
    if(result!=BSR_OK)return bsr_fail(r,result,"duplex metadata");
    result=bsr_remap_init(r,&r->meta_map,r->header+0x690,table_b,table_b_size,r->metadata_duplex.view);
    if(result!=BSR_OK)return result;
    /* Range-check unused journal bitmaps as geometry; their contents are not needed by a reader. */
    for(unsigned i=0;i<3;++i) {
        const size_t position=0xE8+(size_t)i*0x10;
        const uint64_t offset=bsr_u64(layout+position),length=bsr_u64(layout+position+8);
        if(!bsr_range(offset,length,r->meta_map.view.size))return bsr_fail(r,BSR_RANGE,"journal bitmap range");
    }
    return BSR_OK;
}
static BsrResult initialize_integrity(BsrReader *r) {
    const uint8_t *layout=r->header+0x100,*header=r->header+0x344;
    const uint64_t master_size=bsr_u64(layout+0xD0),master_a=bsr_u64(layout+0xC0),master_b=bsr_u64(layout+0xC8);
    const uint64_t additional_start=bsr_u32(layout+4)==0x50000?0xBA0:0xAD8;
    if(master_a<additional_start || master_b<additional_start
       || !bsr_range(master_a,master_size,BSR_HEADER_SIZE) || !bsr_range(master_b,master_size,BSR_HEADER_SIZE))
        return bsr_fail(r,BSR_RANGE,"data IVFC master range");
    View master;BsrResult result=bsr_slice(&r->master_slice,r->header_view,master_a,master_size,&master);
    if(result!=BSR_OK)return bsr_fail(r,result,"data IVFC master");
    View data[4];
    for(unsigned i=0;i<4;++i) {
        const uint8_t *info=header+0x10+(size_t)i*0x18;
        if(i<3 && (bsr_u64(info)!=bsr_u64(layout+0x118+(size_t)i*0x10)
                    || bsr_u64(info+8)!=bsr_u64(layout+0x120+(size_t)i*0x10)))
            return bsr_fail(r,BSR_FORMAT,"data IVFC layout mismatch");
        result=bsr_slice(&r->data_slices[i],i==3?r->journal.view:r->meta_map.view,
                         bsr_u64(info),bsr_u64(info+8),&data[i]);
        if(result!=BSR_OK)return bsr_fail(r,result,"data IVFC range");
    }
    result=bsr_ivfc_init(r,&r->data_ivfc,header,5,master,data);if(result!=BSR_OK)return result;
    result=bsr_ivfc_verify(&r->data_ivfc);if(result!=BSR_OK)return bsr_fail(r,result,"data IVFC verification");
    if(bsr_u32(layout+4)==0x50000) {
        const uint8_t *fat_header=r->header+0xAD8;
        const uint64_t fat_a=bsr_u64(layout+0x160),fat_b=bsr_u64(layout+0x168);
        const uint64_t fat_master_size=bsr_u32(fat_header+8);
        if(fat_a<additional_start || fat_b<additional_start
           || !bsr_range(fat_a,fat_master_size,BSR_HEADER_SIZE) || !bsr_range(fat_b,fat_master_size,BSR_HEADER_SIZE))
            return bsr_fail(r,BSR_RANGE,"FAT IVFC master range");
        result=bsr_slice(&r->fat_master_slice,r->header_view,fat_a,fat_master_size,&master);
        if(result!=BSR_OK)return bsr_fail(r,result,"FAT IVFC master");
        for(unsigned i=0;i<3;++i) {
            const uint8_t *info=fat_header+0x10+(size_t)i*0x18;
            if(i<2 && (bsr_u64(info)!=bsr_u64(layout+0x170+(size_t)i*0x10)
                        || bsr_u64(info+8)!=bsr_u64(layout+0x178+(size_t)i*0x10)))
                return bsr_fail(r,BSR_FORMAT,"FAT IVFC layout mismatch");
            result=bsr_slice(&r->fat_slices[i],r->meta_map.view,bsr_u64(info),bsr_u64(info+8),&data[i]);
            if(result!=BSR_OK)return bsr_fail(r,result,"FAT IVFC range");
        }
        result=bsr_ivfc_init(r,&r->fat_ivfc,fat_header,4,master,data);if(result!=BSR_OK)return result;
        result=bsr_ivfc_verify(&r->fat_ivfc);if(result!=BSR_OK)return bsr_fail(r,result,"FAT IVFC verification");
    }
    return BSR_OK;
}
BsrResult bsr_open(const BsrStorage *storage,BsrValidateCmac validate_cmac,
                   void *cmac_context,BsrReader **out,BsrError *error) {
    if(out!=NULL)*out=NULL;
    if(error!=NULL)*error=(BsrError){BSR_OK,"none"};
    if(storage==NULL || storage->ReadExact==NULL || storage->GetSize==NULL
       || validate_cmac==NULL || out==NULL) {
        if(error!=NULL)*error=(BsrError){BSR_ARGUMENT,"callbacks"};
        return BSR_ARGUMENT;
    }
    BsrReader *r=malloc(sizeof(*r));
    if(r==NULL){if(error!=NULL)*error=(BsrError){BSR_MEMORY,"reader allocation"};return BSR_MEMORY;}
    memset(r,0,sizeof(*r));r->storage=*storage;r->memory_used=sizeof(*r);
    uint64_t size=0;
    if(!storage->GetSize(storage->context,&size)){bsr_fail(r,BSR_IO,"GetSize");goto fail;}
    r->raw=(View){r,raw_read,size};
    if(headers(r,validate_cmac,cmac_context)!=BSR_OK)goto fail;
    if(initialize_maps_and_duplex(r)!=BSR_OK)goto fail;
    if(bsr_journal_init(r)!=BSR_OK)goto fail;
    if(initialize_integrity(r)!=BSR_OK)goto fail;
    if(bsr_filesystem_init(r)!=BSR_OK)goto fail;
    r->ready=true;*out=r;return BSR_OK;
fail:
    {
        const BsrResult result=r->error.result;
        if(error!=NULL)*error=r->error;
        bsr_close(&r);return result;
    }
}
BsrResult bsr_file_size(const BsrReader *r,uint64_t *size) {
    if(r==NULL || size==NULL)return BSR_ARGUMENT;
    if(!r->ready || r->poisoned)return BSR_FORMAT;
    *size=r->target_size;return BSR_OK;
}
BsrResult bsr_file_read(BsrReader *r,uint64_t offset,void *buffer,size_t size) {
    if(r==NULL || (buffer==NULL && size!=0))return BSR_ARGUMENT;
    if(!r->ready || r->poisoned) {
        if(buffer!=NULL)bsr_wipe(buffer,size);
        return BSR_FORMAT;
    }
    if(!bsr_range(offset,size,r->target_size)) {
        if(buffer!=NULL)bsr_wipe(buffer,size);
        return BSR_RANGE;
    }
    const BsrResult result=bsr_read(r->target_chain.view,offset,buffer,size);
    if(result!=BSR_OK){r->poisoned=true;bsr_fail(r,result,"/file read");}
    return result;
}
void bsr_close(BsrReader **reader) {
    if(reader==NULL || *reader==NULL)return;
    BsrReader *r=*reader;*reader=NULL;
    for(size_t i=r->allocations_used;i!=0;) {
        --i;bsr_wipe(r->allocations[i].pointer,r->allocations[i].size);free(r->allocations[i].pointer);
    }
    bsr_wipe(r,sizeof(*r));free(r);
}
const char *bsr_result_string(BsrResult result) {
    static const char *const names[]={"OK","ARGUMENT","RANGE","IO","FORMAT","AUTH","INTEGRITY",
        "LIMIT","MEMORY","CYCLE","NOT_FOUND","AMBIGUOUS","UNSUPPORTED"};
    return (unsigned)result<sizeof(names)/sizeof(names[0])?names[result]:"UNKNOWN";
}
