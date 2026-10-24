#ifndef BSR_INTERNAL_H
#define BSR_INTERNAL_H
#include "bt_save_reader.h"
#include "sha256.h"
enum { BSR_HEADER_SIZE=0x4000, BSR_MAX_ALLOCATIONS=128, BSR_MAX_NODES=65536,
       BSR_MAX_BLOCK_SIZE=65536, BSR_MEMORY_BUDGET=64*1024*1024 };
typedef struct View View;
struct View {
    void *context;
    BsrResult (*read)(void *,uint64_t,void *,size_t);
    uint64_t size;
};
typedef struct { View base; uint64_t offset; } Slice;
typedef struct { const uint8_t *bytes; size_t size; } Bytes;
typedef struct { uint64_t virtual_offset, physical_offset, size; uint32_t alignment; } MapEntry;
typedef struct { uint32_t first,count; } MapSegment;
typedef struct {
    struct BsrReader *owner; View base,view;
    MapEntry *entries; MapSegment *segments; uint32_t count,segment_count,bits;
} Remap;
typedef struct {
    View a,b,view; const uint8_t *bitmap; size_t bitmap_size; uint64_t block_size;
} Duplex;
typedef struct {View base,view;uint64_t block_size;uint32_t *map,count;} Journal;
typedef struct {View base,hash,view;uint64_t block_size;uint8_t salt[32];uint8_t *scratch;} Integrity;
typedef struct {Integrity levels[4];View view;unsigned count;} Ivfc;
typedef struct {View data,view;uint32_t *blocks,count;uint64_t block_size,data_offset;} Chain;
typedef struct {uint8_t *entries;uint32_t count;uint8_t *used;} List;
struct BsrReader {
    BsrStorage storage;
    BsrError error;
    bool authenticated,ready,poisoned;
    struct {void *pointer;size_t size;} allocations[BSR_MAX_ALLOCATIONS];
    size_t allocations_used,memory_used;
    uint8_t header[BSR_HEADER_SIZE];
    View raw,header_view;
    Bytes header_bytes;
    Slice main_slice,master_slice,fat_master_slice;
    Remap main_map,meta_map;
    Duplex bitmap_duplex,metadata_duplex;
    Journal journal;
    Ivfc data_ivfc,fat_ivfc;
    Slice data_slices[4],fat_slices[3];
    uint8_t *fat;uint32_t fat_entries,data_blocks;uint64_t block_size,data_offset;
    Chain directory_chain,file_table_chain,target_chain;
    List directories,files;
    uint64_t target_size;
};
uint32_t bsr_u32(const uint8_t *bytes);
uint64_t bsr_u64(const uint8_t *bytes);
bool bsr_range(uint64_t offset,uint64_t size,uint64_t total);
bool bsr_mul(uint64_t a,uint64_t b,uint64_t *out);
BsrResult bsr_fail(BsrReader *r,BsrResult result,const char *stage);
void *bsr_alloc(BsrReader *r,uint64_t count,size_t unit);
BsrResult bsr_read(View view,uint64_t offset,void *buffer,size_t size);
BsrResult bsr_slice(Slice *slice,View base,uint64_t offset,uint64_t size,View *out);
View bsr_bytes(Bytes *bytes);
BsrResult bsr_remap_init(BsrReader *r,Remap *map,const uint8_t header[0x40],
                         uint64_t table_offset,uint64_t table_size,View base);
BsrResult bsr_duplex_init(Duplex *duplex,View a,View b,const uint8_t *bitmap,
                          size_t bitmap_size,uint32_t block_power);
BsrResult bsr_journal_init(BsrReader *r);
BsrResult bsr_ivfc_init(BsrReader *r,Ivfc *ivfc,const uint8_t header[0xC0],
                       unsigned levels,View master,View *data);
BsrResult bsr_ivfc_verify(Ivfc *ivfc);
BsrResult bsr_chain_init(BsrReader *r,Chain *chain,uint32_t start);
BsrResult bsr_filesystem_init(BsrReader *r);
#endif
