#include "internal.h"
#include <stdlib.h>
#include <string.h>

uint32_t bsr_u32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
uint64_t bsr_u64(const uint8_t *p) {return bsr_u32(p)|((uint64_t)bsr_u32(p+4)<<32);}
bool bsr_range(uint64_t offset,uint64_t size,uint64_t total) {
    return offset<=total && size<=total-offset;
}
bool bsr_mul(uint64_t a,uint64_t b,uint64_t *out) {
    if(b!=0 && a>UINT64_MAX/b)return false;
    *out=a*b;return true;
}
BsrResult bsr_fail(BsrReader *r,BsrResult result,const char *stage) {
    if(r->error.result==BSR_OK)r->error=(BsrError){result,stage};
    return result;
}
void *bsr_alloc(BsrReader *r,uint64_t count,size_t unit) {
    uint64_t size;
    if(count==0 || unit==0 || !bsr_mul(count,unit,&size) || size>SIZE_MAX
       || size>BSR_MEMORY_BUDGET-r->memory_used || r->allocations_used>=BSR_MAX_ALLOCATIONS) {
        bsr_fail(r,BSR_LIMIT,"allocation");return NULL;
    }
    void *p=malloc((size_t)size);
    if(p==NULL){bsr_fail(r,BSR_MEMORY,"allocation");return NULL;}
    memset(p,0,(size_t)size);
    r->allocations[r->allocations_used].pointer=p;
    r->allocations[r->allocations_used].size=(size_t)size;
    ++r->allocations_used;r->memory_used+=(size_t)size;
    return p;
}
BsrResult bsr_read(View view,uint64_t offset,void *buffer,size_t size) {
    if((buffer==NULL && size!=0) || view.read==NULL)return BSR_ARGUMENT;
    if(!bsr_range(offset,size,view.size)) {
        if(buffer!=NULL)bsr_wipe(buffer,size);
        return BSR_RANGE;
    }
    if(size==0)return BSR_OK;
    const BsrResult result=view.read(view.context,offset,buffer,size);
    if(result!=BSR_OK)bsr_wipe(buffer,size);
    return result;
}
static BsrResult slice_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Slice *slice=context;
    if(!bsr_range(offset,size,slice->base.size-slice->offset))return BSR_RANGE;
    return bsr_read(slice->base,slice->offset+offset,buffer,size);
}
BsrResult bsr_slice(Slice *slice,View base,uint64_t offset,uint64_t size,View *out) {
    if(!bsr_range(offset,size,base.size))return BSR_RANGE;
    *slice=(Slice){base,offset};*out=(View){slice,slice_read,size};return BSR_OK;
}
static BsrResult bytes_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Bytes *bytes=context;
    if(!bsr_range(offset,size,bytes->size))return BSR_RANGE;
    memcpy(buffer,bytes->bytes+(size_t)offset,size);return BSR_OK;
}
View bsr_bytes(Bytes *bytes) {return (View){bytes,bytes_read,bytes->size};}

static uint64_t segment_index(uint64_t offset,uint32_t bits) {
    return bits==0 ? 0 : offset>>(64-bits);
}
static BsrResult remap_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Remap *map=context;uint8_t *out=buffer;
    const uint64_t request_offset=offset;
    const size_t request_size=size;
    /* Preflight the ENTIRE mapping, including gaps, before any backend read. */
    for(unsigned pass=0;pass<2;++pass) {
      offset=request_offset;size=request_size;
      while(size!=0) {
        size_t low=0,high=map->count;
        while(low<high) {
            const size_t mid=low+(high-low)/2;
            if(mid>=map->count)return BSR_FORMAT;
            if(map->entries[mid].virtual_offset<=offset)low=mid+1;else high=mid;
        }
        if(low==0)return BSR_RANGE;
        const size_t entry_idx=low-1;
        /* Mandatory bound BEFORE every indexed map entry access. */
        if(entry_idx>=map->count)return BSR_RANGE;
        const MapEntry *entry=&map->entries[entry_idx];
        if(!bsr_range(offset-entry->virtual_offset,1,entry->size))return BSR_RANGE;
        const uint64_t segment=segment_index(offset,map->bits);
        if(segment>=map->segment_count)return BSR_RANGE;
        const MapSegment *seg=&map->segments[segment];
        if(entry_idx<seg->first || entry_idx-seg->first>=seg->count)return BSR_FORMAT;
        const uint64_t position=offset-entry->virtual_offset;
        const uint64_t available=entry->size-position;
        const size_t n=available<size?(size_t)available:size;
        if(!bsr_range(entry->physical_offset,position,map->base.size))return BSR_RANGE;
        if(pass==1) {
            const BsrResult result=bsr_read(map->base,entry->physical_offset+position,out,n);
            if(result!=BSR_OK)return result;
            out+=n;
        }
        offset+=n;size-=n;
      }
    }
    return BSR_OK;
}
BsrResult bsr_remap_init(BsrReader *r,Remap *map,const uint8_t header[0x40],
                         uint64_t table_offset,uint64_t table_size,View base) {
    if(!r->authenticated)return bsr_fail(r,BSR_AUTH,"remap before CMAC");
    const uint32_t count=bsr_u32(header+8),segments=bsr_u32(header+12),bits=bsr_u32(header+16);
    uint64_t needed;
    if(bsr_u32(header)!=UINT32_C(0x50414D52) || bsr_u32(header+4)!=0x10000)
        return bsr_fail(r,BSR_UNSUPPORTED,"remap header");
    if(count==0 || count>BSR_MAX_NODES || segments==0 || segments>count || bits>63
       || (bits==0 && segments!=1) || (bits!=0 && segments>(UINT64_C(1)<<bits)))
        return bsr_fail(r,BSR_FORMAT,"remap geometry");
    if(!bsr_mul(count,0x20,&needed) || needed>table_size || table_offset<0x8000
       || !bsr_range(table_offset,table_size,r->raw.size))
        return bsr_fail(r,BSR_RANGE,"remap table");
    map->entries=bsr_alloc(r,count,sizeof(*map->entries));
    map->segments=bsr_alloc(r,segments,sizeof(*map->segments));
    if(map->entries==NULL || map->segments==NULL)return r->error.result;
    map->base=base;map->owner=r;map->count=count;map->segment_count=segments;map->bits=bits;
    uint64_t last_end=0;uint32_t previous_segment=0;
    for(uint32_t entry_idx=0;entry_idx<count;++entry_idx) {
        uint8_t raw[0x20]={0};
        if(entry_idx>=map->count)return bsr_fail(r,BSR_RANGE,"remap index");
        BsrResult result=bsr_read(r->raw,table_offset+(uint64_t)entry_idx*0x20,raw,sizeof(raw));
        if(result!=BSR_OK)return bsr_fail(r,result,"remap entry read");
        const MapEntry entry={bsr_u64(raw),bsr_u64(raw+8),bsr_u64(raw+16),bsr_u32(raw+24)};
        bsr_wipe(raw,sizeof(raw));
        if(entry.size==0 || !bsr_range(entry.virtual_offset,entry.size,UINT64_MAX)
           || !bsr_range(entry.physical_offset,entry.size,base.size))
            return bsr_fail(r,BSR_RANGE,"remap entry range");
        /* TegraExplorer remap_storage.h defines byte alignments 0x200 and 0x4000. */
        if((entry.alignment!=0x200 && entry.alignment!=0x4000)
           || entry.virtual_offset%entry.alignment!=0 || entry.physical_offset%entry.alignment!=0
           || entry.size%entry.alignment!=0)return bsr_fail(r,BSR_FORMAT,"remap alignment");
        const uint64_t segment=segment_index(entry.virtual_offset,bits);
        const uint64_t end_segment=segment_index(entry.virtual_offset+entry.size-1,bits);
        if(segment>=segments || segment!=end_segment)
            return bsr_fail(r,BSR_FORMAT,"remap segment index");
        if(entry_idx==0) {
            if(segment!=0 || entry.virtual_offset!=0)return bsr_fail(r,BSR_FORMAT,"remap first segment");
        } else if(segment==previous_segment) {
            if(entry.virtual_offset!=last_end)return bsr_fail(r,BSR_FORMAT,"remap gap or overlap");
        } else {
            if(segment!=previous_segment+1 || bits==0
               || entry.virtual_offset!=(segment<<(64-bits)))
                return bsr_fail(r,BSR_FORMAT,"remap segment geometry");
        }
        MapSegment *seg=&map->segments[segment];
        if(seg->count==0)seg->first=entry_idx;
        ++seg->count;map->entries[entry_idx]=entry;
        previous_segment=(uint32_t)segment;last_end=entry.virtual_offset+entry.size;
    }
    if(previous_segment!=segments-1)return bsr_fail(r,BSR_FORMAT,"remap missing segment");
    map->view=(View){map,remap_read,last_end};return BSR_OK;
}

static BsrResult duplex_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Duplex *duplex=context;uint8_t *out=buffer;
    while(size!=0) {
        const uint64_t block=offset/duplex->block_size;
        const uint64_t word_index=block/32;
        uint64_t word_offset;
        if(!bsr_mul(word_index,4,&word_offset) || !bsr_range(word_offset,4,duplex->bitmap_size))return BSR_RANGE;
        const uint32_t word=bsr_u32(duplex->bitmap+(size_t)word_offset);
        const bool use_b=(word&(UINT32_C(1)<<(31-(unsigned)(block%32))))!=0;
        const uint64_t available=duplex->block_size-offset%duplex->block_size;
        const size_t n=available<size?(size_t)available:size;
        BsrResult result=bsr_read(use_b?duplex->b:duplex->a,offset,out,n);
        if(result!=BSR_OK)return result;
        offset+=n;out+=n;size-=n;
    }
    return BSR_OK;
}
BsrResult bsr_duplex_init(Duplex *duplex,View a,View b,const uint8_t *bitmap,
                          size_t bitmap_size,uint32_t block_power) {
    if(a.size==0 || a.size!=b.size || bitmap==NULL || block_power>30)return BSR_FORMAT;
    const uint64_t block_size=UINT64_C(1)<<block_power;
    const uint64_t blocks=a.size/block_size+(a.size%block_size!=0);
    const uint64_t words=blocks/32+(blocks%32!=0);
    uint64_t needed;
    if(!bsr_mul(words,4,&needed) || needed>bitmap_size)return BSR_RANGE;
    *duplex=(Duplex){.a=a,.b=b,.bitmap=bitmap,.bitmap_size=bitmap_size,.block_size=block_size};
    duplex->view=(View){duplex,duplex_read,a.size};return BSR_OK;
}

static BsrResult journal_read(void *context,uint64_t offset,void *buffer,size_t size) {
    Journal *journal=context;uint8_t *out=buffer;
    while(size!=0) {
        const uint64_t index=offset/journal->block_size,position=offset%journal->block_size;
        if(index>=journal->count)return BSR_RANGE;
        uint64_t physical;
        if(!bsr_mul(journal->map[index],journal->block_size,&physical)
           || !bsr_range(physical,position,journal->base.size))return BSR_RANGE;
        const uint64_t available=journal->block_size-position;
        const size_t n=available<size?(size_t)available:size;
        BsrResult result=bsr_read(journal->base,physical+position,out,n);
        if(result!=BSR_OK)return result;
        offset+=n;out+=n;size-=n;
    }
    return BSR_OK;
}
BsrResult bsr_journal_init(BsrReader *r) {
    const uint8_t *h=r->header+0x408,*layout=r->header+0x100;
    const uint64_t total=bsr_u64(h+8),reserved=bsr_u64(h+16),block=bsr_u64(h+24);
    const uint32_t count=bsr_u32(h+0x24),journal_count=bsr_u32(h+0x28);
    uint64_t expected,table_bytes;
    if(bsr_u32(h)!=UINT32_C(0x4C474E4A) || bsr_u32(h+4)!=0x10000 || bsr_u32(h+0x20)>1)
        return bsr_fail(r,BSR_UNSUPPORTED,"journal header");
    if(block==0 || block>BSR_MAX_BLOCK_SIZE || (block&(block-1))!=0 || total<reserved
       || total%block!=0 || reserved%block!=0 || count==0 || count>BSR_MAX_NODES
       || journal_count>BSR_MAX_NODES || !bsr_mul(count,block,&expected)
       || expected!=total-reserved || journal_count!=reserved/block)
        return bsr_fail(r,BSR_FORMAT,"journal geometry");
    if(expected>BSR_MEMORY_BUDGET)return bsr_fail(r,BSR_LIMIT,"journal data budget");
    const uint64_t journal_offset=bsr_u64(layout+0x88);
    if(!bsr_range(journal_offset,bsr_u64(layout+0x90),r->main_map.view.size)
       || !bsr_range(journal_offset,bsr_u64(layout+0x98),r->main_map.view.size)
       || bsr_u64(layout+0xA0)!=reserved)
        return bsr_fail(r,BSR_FORMAT,"journal layout geometry");
    if(!bsr_mul(count,8,&table_bytes) || table_bytes>bsr_u64(layout+0xE0))
        return bsr_fail(r,BSR_RANGE,"journal map size");
    /* Store the validated data slice in reader-owned allocation, not a temporary. */
    Slice *data_slice=bsr_alloc(r,1,sizeof(*data_slice));
    if(data_slice==NULL)return r->error.result;
    BsrResult result=bsr_slice(data_slice,r->main_map.view,bsr_u64(layout+0x88),total,&r->journal.base);
    if(result!=BSR_OK)return bsr_fail(r,result,"journal data range");
    if(!bsr_range(bsr_u64(layout+0xD8),bsr_u64(layout+0xE0),r->meta_map.view.size))
        return bsr_fail(r,BSR_RANGE,"journal map range");
    r->journal.map=bsr_alloc(r,count,sizeof(*r->journal.map));
    uint8_t *seen=bsr_alloc(r,count+(uint64_t)journal_count,1);
    if(r->journal.map==NULL || seen==NULL)return r->error.result;
    for(uint32_t i=0;i<count;++i) {
        uint8_t raw[8]={0};
        result=bsr_read(r->meta_map.view,bsr_u64(layout+0xD8)+(uint64_t)i*8,raw,8);
        if(result!=BSR_OK)return bsr_fail(r,result,"journal map read");
        const uint32_t physical=bsr_u32(raw)&UINT32_C(0x7FFFFFFF);
        bsr_wipe(raw,8);
        if(physical>=count+(uint64_t)journal_count || seen[physical]!=0)
            return bsr_fail(r,BSR_FORMAT,"journal physical index");
        seen[physical]=1;r->journal.map[i]=physical;
    }
    r->journal.count=count;r->journal.block_size=block;
    r->journal.view=(View){&r->journal,journal_read,expected};return BSR_OK;
}
