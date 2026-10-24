#include "internal.h"
#include <string.h>

static BsrResult integrity_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Integrity *layer=context;uint8_t *out=buffer;
    while(size!=0) {
        const uint64_t block=offset/layer->block_size;
        uint64_t hash_offset,block_offset;
        if(!bsr_mul(block,32,&hash_offset) || !bsr_mul(block,layer->block_size,&block_offset)
           || !bsr_range(hash_offset,32,layer->hash.size) || block_offset>=layer->base.size)
            return BSR_RANGE;
        uint8_t expected[32]={0},actual[32]={0};
        BsrResult result=bsr_read(layer->hash,hash_offset,expected,sizeof(expected));
        if(result!=BSR_OK)return result;
        const uint64_t available=layer->base.size-block_offset;
        const size_t stored=available<layer->block_size?(size_t)available:(size_t)layer->block_size;
        memset(layer->scratch,0,(size_t)layer->block_size);
        bool empty=true;
        for(size_t i=0;i<32;++i)if(expected[i]!=0)empty=false;
        /* An authenticated all-zero hash denotes a logical zero sector in this format. */
        if(!empty) {
            result=bsr_read(layer->base,block_offset,layer->scratch,stored);
            if(result!=BSR_OK)goto cleanup;
            BsrSha sha;bsr_sha_init(&sha);bsr_sha_update(&sha,layer->salt,32);
            bsr_sha_update(&sha,layer->scratch,(size_t)layer->block_size);bsr_sha_final(&sha,actual);
            actual[31]|=0x80;
            if(memcmp(actual,expected,32)!=0){result=BSR_INTEGRITY;goto cleanup;}
        }
        const uint64_t position=offset-block_offset;
        const uint64_t remain=stored-position;
        const size_t n=remain<size?(size_t)remain:size;
        if(n==0){result=BSR_RANGE;goto cleanup;}
        memcpy(out,layer->scratch+(size_t)position,n);
        out+=n;offset+=n;size-=n;
cleanup:
        bsr_wipe(layer->scratch,(size_t)layer->block_size);
        bsr_wipe(expected,sizeof(expected));bsr_wipe(actual,sizeof(actual));
        if(result!=BSR_OK)return result;
    }
    return BSR_OK;
}
BsrResult bsr_ivfc_init(BsrReader *r,Ivfc *ivfc,const uint8_t header[0xC0],
                       unsigned levels,View master,View *data) {
    static const char *const labels[4]={
        "HierarchicalIntegrityVerificationStorage::Master",
        "HierarchicalIntegrityVerificationStorage::L1",
        "HierarchicalIntegrityVerificationStorage::L2",
        "HierarchicalIntegrityVerificationStorage::L3"
    };
    static const size_t label_sizes[4]={48,44,44,44}; /* Reference lengths exclude terminating NUL. */
    const uint32_t encoded_levels=bsr_u32(header+12);
    if(levels<2 || levels>5 || bsr_u32(header)!=UINT32_C(0x43465649)
       || bsr_u32(header+4)!=0x20000 || (encoded_levels!=0 && encoded_levels!=levels))
        return bsr_fail(r,BSR_UNSUPPORTED,"IVFC header/levels");
    if(master.size==0 || master.size%32!=0 || bsr_u32(header+8)!=master.size)
        return bsr_fail(r,BSR_FORMAT,"IVFC master size");
    View parent=master;
    /* Geometry of ALL levels is validated before allocating/reading any level. */
    for(unsigned i=0;i<levels-1;++i) {
        const uint8_t *h=header+0x10+(size_t)i*0x18;
        const uint32_t power=bsr_u32(h+16);
        if(power<5 || power>16 || data[i].size==0 || data[i].size!=bsr_u64(h+8))
            return bsr_fail(r,BSR_FORMAT,"IVFC geometry");
        const uint64_t block=UINT64_C(1)<<power;
        const uint64_t sectors=data[i].size/block+(data[i].size%block!=0);
        uint64_t needed;
        if(sectors>BSR_MAX_NODES || !bsr_mul(sectors,32,&needed) || needed>parent.size)
            return bsr_fail(r,BSR_RANGE,"IVFC hash geometry");
        parent=data[i];
    }
    const unsigned count=levels-1;
    parent=master;ivfc->count=count;
    for(unsigned i=0;i<count && i<4;++i) {
        Integrity *layer=&ivfc->levels[i];
        layer->base=data[i];layer->hash=parent;
        layer->block_size=UINT64_C(1)<<bsr_u32(header+0x10+(size_t)i*0x18+16);
        layer->scratch=bsr_alloc(r,layer->block_size,1);
        if(layer->scratch==NULL)return r->error.result;
        bsr_hmac256(header+0xA0,labels[i],label_sizes[i],layer->salt);
        layer->view=(View){layer,integrity_read,data[i].size};parent=layer->view;
    }
    ivfc->view=parent;return BSR_OK;
}
BsrResult bsr_ivfc_verify(Ivfc *ivfc) {
    /* Eagerly verify every sector of every level before constructing filesystem objects.
     * Reads are reverified later too; no unchecked-validity/cache bypass exists. */
    if(ivfc->count==0 || ivfc->count>4)return BSR_FORMAT;
    const unsigned count=ivfc->count;
    uint8_t byte;
    for(unsigned level=0;level<count && level<4;++level) {
        const Integrity *layer=&ivfc->levels[level];
        uint64_t offset=0;
        while(offset<layer->view.size) {
            const BsrResult result=bsr_read(layer->view,offset,&byte,1);
            bsr_wipe(&byte,1);
            if(result!=BSR_OK)return result;
            const uint64_t remaining=layer->view.size-offset;
            if(remaining<=layer->block_size)break;
            offset+=layer->block_size;
        }
    }
    return BSR_OK;
}
