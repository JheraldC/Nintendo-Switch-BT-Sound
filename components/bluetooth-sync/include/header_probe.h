/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_HEADER_PROBE_H
#define BTD_HEADER_PROBE_H
#include "bt_save_reader.h"
typedef bool (*BtdComputeCmac)(void *,const uint8_t *,size_t,uint8_t out[16]);
typedef struct { bool read,cmac_engine,cmac_match,magic,version_ok,sha_match;uint32_t version; } BtdHeaderCheck;
enum { BTD_DPFS,BTD_DATA_IVFC,BTD_JOURNAL,BTD_SAVE,BTD_MAIN_RMAP,BTD_META_RMAP,BTD_FAT_IVFC,BTD_LAYOUT_PARTS };
typedef struct {
    BtdHeaderCheck header[2];bool identical;uint64_t container_size;
    /* Format verdicts only; no header contents, hashes, identifiers or keys. */
    bool layout_checked;uint32_t layout_mask,layout_valid,layout_version[BTD_LAYOUT_PARTS];
    uint32_t remap_geometry[2][3],ivfc_levels[2];
} BtdHeaderSummary;
/* Only reads the two fixed metadata headers. No payload /file reads, dumps,
   repairs or alternative-header selection. Independent A/B diagnostics. */
BsrResult btd_probe_headers(const BsrStorage *,BtdComputeCmac,void *,BtdHeaderSummary *);
#endif
