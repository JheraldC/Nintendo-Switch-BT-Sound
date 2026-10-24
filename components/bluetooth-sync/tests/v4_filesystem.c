/* SPDX-License-Identifier: GPL-2.0-only
 * Reviewed v4 derivative of the frozen reference filesystem.c.
 * See output/filesystem-reference.diff and tests/test_fs_geometry.c.
 * Authentication, IVFC and transport restrictions remain mandatory.
 */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
extern void btd_status(const char *,int);
extern void btd_number(const char *,uint32_t);
extern void btd_skip(const char *);
static void geometry_number(const char *name,const char *high,uint64_t value) {
    btd_number(name,(uint32_t)value);if(value>>32)btd_number(high,(uint32_t)(value>>32));
}
static void geometry_check(const char *stage,bool ok,BsrResult code,BsrError *failure) {
    btd_status(stage,ok?BSR_OK:code);
    if(!ok&&failure->result==BSR_OK)*failure=(BsrError){code,stage};
}

static BsrResult chain_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Chain *chain=context;uint8_t *out=buffer;
    while(size!=0) {
        const uint64_t index=offset/chain->block_size,position=offset%chain->block_size;
        if(index>=chain->count)return BSR_RANGE;
        uint64_t physical;
        if(!bsr_mul(chain->blocks[index],chain->block_size,&physical)
           || !bsr_range(physical,chain->data_offset,chain->data.size))return BSR_RANGE;
        physical+=chain->data_offset;
        if(!bsr_range(physical,position,chain->data.size))return BSR_RANGE;
        const uint64_t available=chain->block_size-position;
        const size_t n=available<size?(size_t)available:size;
        const BsrResult result=bsr_read(chain->data,physical+position,out,n);
        if(result!=BSR_OK)return result;
        out+=n;offset+=n;size-=n;
    }
    return BSR_OK;
}
static const uint8_t *fat_entry(const BsrReader *r,uint32_t index) {
    return index<r->fat_entries ? r->fat+(size_t)index*8 : NULL;
}
BsrResult bsr_chain_init(BsrReader *r,Chain *chain,uint32_t start) {
    memset(chain,0,sizeof(*chain));
    chain->data=r->data_ivfc.view;chain->data_offset=r->data_offset;chain->block_size=r->block_size;
    if(start==UINT32_C(0xFFFFFFFF)) {chain->view=(View){chain,chain_read,0};return BSR_OK;}
    if(start>=r->data_blocks)return bsr_fail(r,BSR_RANGE,"FAT initial block");
    uint8_t *seen=bsr_alloc(r,r->data_blocks,1);
    chain->blocks=bsr_alloc(r,r->data_blocks,sizeof(*chain->blocks));
    if(seen==NULL || chain->blocks==NULL)return r->error.result;
    uint32_t index=start+1,previous=UINT32_C(0x80000000),visits=0;
    while(index!=0) {
        if(index>=r->fat_entries || index-1>=r->data_blocks || ++visits>r->data_blocks)
            return bsr_fail(r,BSR_RANGE,"FAT index/visit limit");
        const uint8_t *entry=fat_entry(r,index);
        if(entry==NULL)return bsr_fail(r,BSR_RANGE,"FAT entry");
        const uint32_t prev=bsr_u32(entry),next=bsr_u32(entry+4);
        if(prev!=previous)return bsr_fail(r,BSR_FORMAT,"FAT backlink");
        uint32_t end=index;
        if((next&UINT32_C(0x80000000))!=0) {
            if(index+1>=r->fat_entries)return bsr_fail(r,BSR_RANGE,"FAT range descriptor");
            const uint8_t *range=fat_entry(r,index+1);
            if(range==NULL)return bsr_fail(r,BSR_RANGE,"FAT descriptor");
            end=bsr_u32(range+4);
            if(bsr_u32(range)!=(index|UINT32_C(0x80000000)) || end<=index
               || end>=r->fat_entries || end-1>=r->data_blocks)
                return bsr_fail(r,BSR_FORMAT,"FAT range geometry");
            const uint8_t *tail=fat_entry(r,end);
            if(tail==NULL || memcmp(range,tail,8)!=0)return bsr_fail(r,BSR_FORMAT,"FAT range tail");
        }
        for(uint32_t i=index;i<=end;++i) {
            const uint32_t block=i-1;
            if(block>=r->data_blocks || chain->count>=r->data_blocks)
                return bsr_fail(r,BSR_RANGE,"FAT chain length");
            if(seen[block]!=0)return bsr_fail(r,BSR_CYCLE,"FAT cycle/overlap");
            seen[block]=1;chain->blocks[chain->count++]=block;
        }
        previous=index;index=next&UINT32_C(0x7FFFFFFF);
        if(index!=0 && index-1>=r->data_blocks)return bsr_fail(r,BSR_RANGE,"FAT next");
        if(index!=0 && seen[index-1]!=0)return bsr_fail(r,BSR_CYCLE,"FAT cycle");
    }
    uint64_t length;
    if(!bsr_mul(chain->count,chain->block_size,&length) || length>chain->data.size)
        return bsr_fail(r,BSR_RANGE,"FAT chain size");
    chain->view=(View){chain,chain_read,length};return BSR_OK;
}
static const uint8_t *list_entry(const List *list,uint32_t index) {
    return index<list->count ? list->entries+(size_t)index*0x60 : NULL;
}
static BsrResult list_init(BsrReader *r,List *list,View view) {
    uint8_t header[8]={0};
    BsrResult result=bsr_read(view,0,header,8);
    if(result!=BSR_OK)return bsr_fail(r,result,"list header");
    const uint32_t length=bsr_u32(header),capacity=bsr_u32(header+4);
    uint64_t bytes;
    if(length<2 || length>capacity || capacity>BSR_MAX_NODES
       || !bsr_mul(capacity,0x60,&bytes) || bytes>view.size)
        return bsr_fail(r,BSR_FORMAT,"list capacity");
    list->entries=bsr_alloc(r,length,0x60);list->used=bsr_alloc(r,length,1);list->count=length;
    if(list->entries==NULL || list->used==NULL)return r->error.result;
    result=bsr_read(view,0,list->entries,(size_t)length*0x60);
    if(result!=BSR_OK)return bsr_fail(r,result,"list snapshot");
    if(memcmp(header,list->entries,8)!=0)return bsr_fail(r,BSR_FORMAT,"list changed during read");
    for(unsigned head=0;head<2;++head) {
        const uint8_t *sentinel=list_entry(list,head);
        if(sentinel==NULL)return bsr_fail(r,BSR_RANGE,"list sentinel");
        uint32_t index=bsr_u32(sentinel+0x5C),visits=0;
        while(index!=0) {
            if(index<2 || index>=length || ++visits>length-2)
                return bsr_fail(r,BSR_RANGE,"list index/visit limit");
            if(list->used[index]!=0)return bsr_fail(r,BSR_CYCLE,"list cycle/overlap");
            list->used[index]=(uint8_t)(head==1?1:2);
            const uint8_t *entry=list_entry(list,index);
            if(entry==NULL)return bsr_fail(r,BSR_RANGE,"list entry");
            if(head==1 && memchr(entry+4,0,0x40)==NULL)
                return bsr_fail(r,BSR_FORMAT,"list non-terminated name");
            index=bsr_u32(entry+0x5C);
        }
    }
    for(uint32_t index=2;index<length;++index)if(list->used[index]==0)
        return bsr_fail(r,BSR_FORMAT,"list orphan node");
    return BSR_OK;
}
static bool named(const uint8_t *entry,const char *name,size_t size) {
    const uint8_t *end=memchr(entry+4,0,0x40);
    return end!=NULL && (size_t)(end-(entry+4))==size && memcmp(entry+4,name,size)==0;
}
typedef struct {uint32_t parent;const uint8_t *name;size_t length;} Key;
static int compare_key(const void *a,const void *b) {
    const Key *ka=a,*kb=b;
    if(ka->parent!=kb->parent)return ka->parent<kb->parent?-1:1;
    const size_t n=ka->length<kb->length?ka->length:kb->length;
    const int result=memcmp(ka->name,kb->name,n);
    if(result!=0)return result;
    return ka->length==kb->length?0:(ka->length<kb->length?-1:1);
}
static BsrResult directory_graph(BsrReader *r,uint32_t *root_out) {
    const List *dirs=&r->directories,*files=&r->files;uint32_t root=0,roots=0;
    for(uint32_t i=2;i<dirs->count;++i)if(dirs->used[i]==1) {
        const uint8_t *entry=list_entry(dirs,i);
        if(entry==NULL)return bsr_fail(r,BSR_RANGE,"directory index");
        if(bsr_u32(entry)==0 && named(entry,"",0)) {root=i;++roots;}
    }
    if(roots!=1)return bsr_fail(r,BSR_AMBIGUOUS,"root directory");
    uint8_t *states=bsr_alloc(r,dirs->count,1);
    uint32_t *stack=bsr_alloc(r,dirs->count,sizeof(*stack));
    uint8_t *dir_children=bsr_alloc(r,dirs->count,1),*file_children=bsr_alloc(r,files->count,1);
    Key *keys=bsr_alloc(r,(uint64_t)dirs->count+files->count,sizeof(*keys));
    if(states==NULL || stack==NULL || dir_children==NULL || file_children==NULL || keys==NULL)
        return r->error.result;
    states[root]=2;size_t key_count=0;
    for(uint32_t i=2;i<dirs->count;++i)if(dirs->used[i]==1) {
        uint32_t index=i,depth=0;
        while(index!=root) {
            if(index<2 || index>=dirs->count || dirs->used[index]!=1 || depth>=dirs->count)
                return bsr_fail(r,BSR_RANGE,"directory parent index");
            if(states[index]==2)break;
            if(states[index]==1)return bsr_fail(r,BSR_CYCLE,"directory parent cycle");
            states[index]=1;stack[depth++]=index;
            const uint8_t *entry=list_entry(dirs,index);
            if(entry==NULL)return bsr_fail(r,BSR_RANGE,"directory parent entry");
            index=bsr_u32(entry);
        }
        while(depth!=0)states[stack[--depth]]=2;
    }
    for(unsigned type=0;type<2;++type) {
        const List *list=type==0?dirs:files;
        for(uint32_t i=2;i<list->count;++i)if(list->used[i]==1) {
            const uint8_t *entry=list_entry(list,i);
            if(entry==NULL)return bsr_fail(r,BSR_RANGE,"namespace entry");
            const uint8_t *end=memchr(entry+4,0,0x40);
            if(end==NULL)return bsr_fail(r,BSR_FORMAT,"namespace string");
            const size_t length=(size_t)(end-(entry+4));
            const uint32_t parent=bsr_u32(entry);
            if(!(type==0 && i==root) && (length==0 || parent<2 || parent>=dirs->count || dirs->used[parent]!=1))
                return bsr_fail(r,BSR_FORMAT,"namespace parent/name");
            if(memchr(entry+4,'/',length)!=NULL || named(entry,".",1) || named(entry,"..",2))
                return bsr_fail(r,BSR_FORMAT,"namespace forbidden component");
            keys[key_count++]=(Key){parent,entry+4,length};
        }
    }
    qsort(keys,key_count,sizeof(*keys),compare_key);
    for(size_t i=1;i<key_count;++i)if(compare_key(&keys[i-1],&keys[i])==0)
        return bsr_fail(r,BSR_AMBIGUOUS,"duplicate namespace key");
    for(uint32_t parent=2;parent<dirs->count;++parent)if(dirs->used[parent]==1) {
        const uint8_t *entry=list_entry(dirs,parent);
        if(entry==NULL)return bsr_fail(r,BSR_RANGE,"parent entry");
        for(unsigned type=0;type<2;++type) {
            const List *list=type==0?dirs:files;uint8_t *seen=type==0?dir_children:file_children;
            uint32_t index=bsr_u32(entry+(type==0?0x48:0x4C)),visits=0;
            while(index!=0) {
                if(index<2 || index>=list->count || list->used[index]!=1 || ++visits>list->count)
                    return bsr_fail(r,BSR_RANGE,"child index/visit limit");
                if(seen[index]!=0)return bsr_fail(r,BSR_CYCLE,"child cycle/duplicate");
                seen[index]=1;
                const uint8_t *child=list_entry(list,index);
                if(child==NULL || bsr_u32(child)!=parent)
                    return bsr_fail(r,BSR_FORMAT,"child parent mismatch");
                index=bsr_u32(child+0x44);
            }
        }
    }
    for(uint32_t i=2;i<dirs->count;++i)if(dirs->used[i]==1 && i!=root && dir_children[i]!=1)
        return bsr_fail(r,BSR_FORMAT,"unlinked directory");
    if(dir_children[root]!=0 || bsr_u32(list_entry(dirs,root)+0x44)!=0)
        return bsr_fail(r,BSR_FORMAT,"root sibling");
    for(uint32_t i=2;i<files->count;++i)if(files->used[i]==1 && file_children[i]!=1)
        return bsr_fail(r,BSR_FORMAT,"unlinked file");
    *root_out=root;return BSR_OK;
}
static BsrResult claim_chain(BsrReader *r,const Chain *chain,uint8_t *owners) {
    for(uint32_t i=0;i<chain->count;++i) {
        const uint32_t block=chain->blocks[i];
        if(block>=r->data_blocks)return bsr_fail(r,BSR_RANGE,"allocation owner index");
        if(owners[block]!=0)return bsr_fail(r,BSR_FORMAT,"cross-linked allocation");
        owners[block]=1;
    }
    return BSR_OK;
}
BsrResult bsr_filesystem_init(BsrReader *r) {
    const uint8_t *h=r->header+0x608,*fat=h+0x18;
    if(bsr_u32(h)!=UINT32_C(0x45564153) || bsr_u32(h+4)!=0x60000)
        return bsr_fail(r,BSR_UNSUPPORTED,"SAVE header");
    btd_status("FS_GEOMETRY",-1);
    r->block_size=bsr_u64(h+16);r->data_blocks=bsr_u32(fat+32);r->data_offset=bsr_u64(fat+24);
    const uint32_t capacity=bsr_u32(fat+16);
    geometry_number("FS_BLOCK_BYTES","FS_BLOCK_HIGH",r->block_size);
    geometry_number("FAT_BLOCK_BYTES","FAT_BLOCK_HIGH",bsr_u64(fat));
    geometry_number("SAVE_INITIAL_BLOCKS","SAVE_INITIAL_HIGH",bsr_u64(h+8));
    btd_number("FAT_DECLARED_ENTRIES",capacity);btd_number("FS_DATA_BLOCKS",r->data_blocks);
    geometry_number("FS_DATA_OFFSET","FS_DATA_OFFSET_HIGH",r->data_offset);
    geometry_number("FAT_CONTROL_OFFSET","FAT_CONTROL_HIGH",bsr_u64(fat+8));
    geometry_number("FS_IVFC_BYTES","FS_IVFC_HIGH",r->data_ivfc.view.size);
    View fat_view=r->fat_ivfc.view;
    if(bsr_u32(r->header+0x104)==0x40000) {
        Slice *legacy_slice=bsr_alloc(r,1,sizeof(*legacy_slice));
        if(legacy_slice==NULL)return r->error.result;
        BsrResult result=bsr_slice(legacy_slice,r->meta_map.view,bsr_u64(r->header+0x248),
                                 bsr_u64(r->header+0x250),&fat_view);
        if(result!=BSR_OK)return bsr_fail(r,result,"legacy FAT range");
    }
    geometry_number("FAT_VERIFIED_BYTES","FAT_VERIFIED_HIGH",fat_view.size);
    /* The current data region determines the addressable chains. A reserved
       FAT entry zero is needed in addition to those data blocks. The declared
       capacity can be larger, provided its bytes fit the verified FAT view.
       SAVE +8 is historical (unchanged by resizing), not a current size gate. */
    const bool block_ok=r->block_size!=0&&r->block_size<=BSR_MAX_BLOCK_SIZE&&
                        (r->block_size&(r->block_size-1))==0;
    const bool count_ok=r->data_blocks!=0&&r->data_blocks<=BSR_MAX_NODES;
    uint64_t data_size=0,capacity_bytes=0;
    BsrError failure={BSR_OK,NULL};
    geometry_check("FS_BLOCK_POWER",block_ok,BSR_FORMAT,&failure);
    if(block_ok)geometry_check("FS_BLOCK_MATCH",r->block_size==bsr_u64(fat),BSR_FORMAT,&failure);
    else btd_skip("FS_BLOCK_MATCH");
    geometry_check("FS_DATA_COUNT",count_ok,BSR_FORMAT,&failure);
    if(count_ok)geometry_check("FS_FAT_CAPACITY",(uint64_t)capacity>r->data_blocks,BSR_FORMAT,&failure);
    else btd_skip("FS_FAT_CAPACITY");
    if(count_ok&&block_ok)geometry_check("FS_DATA_RANGE",bsr_mul(r->data_blocks,r->block_size,&data_size)&&
                                       data_size<=r->data_ivfc.view.size,BSR_FORMAT,&failure);
    else btd_skip("FS_DATA_RANGE");
    /* Retain the known control-area profile. No offsets are guessed or fixed up. */
    geometry_check("FS_CONTROL_OFFSETS",bsr_u64(fat+8)==0&&r->data_offset==0,BSR_UNSUPPORTED,&failure);
    geometry_check("FS_FAT_STORAGE",bsr_mul(capacity,8,&capacity_bytes)&&capacity_bytes<=fat_view.size,BSR_RANGE,&failure);
    if(failure.result!=BSR_OK){btd_status("FS_GEOMETRY",(int)failure.result);return bsr_fail(r,failure.result,failure.stage);}
    r->fat_entries=r->data_blocks+1;
    uint64_t fat_size=(uint64_t)r->fat_entries*8;
    btd_number("FAT_REQUIRED_ENTRIES",r->fat_entries);btd_status("FS_GEOMETRY",0);
    r->fat=bsr_alloc(r,r->fat_entries,8);if(r->fat==NULL)return r->error.result;
    BsrResult result=bsr_read(fat_view,0,r->fat,(size_t)fat_size);
    if(result!=BSR_OK)return bsr_fail(r,result,"FAT verified read");
    uint8_t *owners=bsr_alloc(r,r->data_blocks,1);if(owners==NULL)return r->error.result;
    result=bsr_chain_init(r,&r->directory_chain,bsr_u32(fat+40));
    if(result!=BSR_OK)return result;
    btd_number("DIR_CHAIN_BLOCKS",r->directory_chain.count);
    result=claim_chain(r,&r->directory_chain,owners);if(result!=BSR_OK)return result;
    result=bsr_chain_init(r,&r->file_table_chain,bsr_u32(fat+44));
    if(result!=BSR_OK)return result;
    btd_number("FILETAB_CHAIN_BLOCKS",r->file_table_chain.count);
    result=claim_chain(r,&r->file_table_chain,owners);if(result!=BSR_OK)return result;
    result=list_init(r,&r->directories,r->directory_chain.view);if(result!=BSR_OK)return result;
    btd_number("DIR_TABLE_NODES",r->directories.count);
    result=list_init(r,&r->files,r->file_table_chain.view);if(result!=BSR_OK)return result;
    btd_number("FILE_TABLE_NODES",r->files.count);
    uint32_t root=0;result=directory_graph(r,&root);if(result!=BSR_OK)return result;
    unsigned matches=0;
    for(uint32_t i=2;i<r->files.count;++i)if(r->files.used[i]==1) {
        const uint8_t *entry=list_entry(&r->files,i);
        if(entry==NULL)return bsr_fail(r,BSR_RANGE,"file entry");
        Chain *chain=bsr_alloc(r,1,sizeof(*chain));if(chain==NULL)return r->error.result;
        result=bsr_chain_init(r,chain,bsr_u32(entry+0x48));if(result!=BSR_OK)return result;
        result=claim_chain(r,chain,owners);if(result!=BSR_OK)return result;
        const uint64_t length=bsr_u64(entry+0x4C);
        if(length>chain->view.size || (length==0 && chain->view.size!=0))
            return bsr_fail(r,BSR_RANGE,"file length");
        if(bsr_u32(entry)==root && named(entry,"file",4)) {
            ++matches;r->target_chain=*chain;
            /* Copying a view must update its context to the reader-owned target. */
            r->target_chain.view.context=&r->target_chain;r->target_size=length;
        }
    }
    const uint32_t free_index=bsr_u32(r->fat+4)&UINT32_C(0x7FFFFFFF);
    if(free_index!=0) {
        Chain *free_chain=bsr_alloc(r,1,sizeof(*free_chain));if(free_chain==NULL)return r->error.result;
        result=bsr_chain_init(r,free_chain,free_index-1);if(result!=BSR_OK)return result;
        result=claim_chain(r,free_chain,owners);if(result!=BSR_OK)return result;
    }
    for(uint32_t i=0;i<r->data_blocks;++i)if(owners[i]!=1)
        return bsr_fail(r,BSR_FORMAT,"orphan allocation");
    if(matches==0)return bsr_fail(r,BSR_NOT_FOUND,"/file");
    if(matches!=1)return bsr_fail(r,BSR_AMBIGUOUS,"/file");
    btd_status("FS_NAMESPACE",0);
    return BSR_OK;
}
