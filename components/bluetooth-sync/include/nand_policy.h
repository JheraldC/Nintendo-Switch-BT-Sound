/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_NAND_POLICY_H
#define BTD_NAND_POLICY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t start,end;bool ready; } BtdWindow;
typedef enum {
    BTD_GPT_OK=0,BTD_GPT_ARGUMENT=101,BTD_GPT_SIGNATURE,BTD_GPT_REVISION,
    BTD_GPT_HEADER_SIZE,BTD_GPT_RESERVED,BTD_GPT_PRIMARY_LBA,BTD_GPT_BACKUP_LBA,
    BTD_GPT_USABLE_RANGE,BTD_GPT_TABLE_LBA,BTD_GPT_ENTRY_COUNT,BTD_GPT_ENTRY_SIZE,
    BTD_GPT_HEADER_CRC,BTD_GPT_TABLE_CRC,BTD_GPT_PART_RANGE,BTD_GPT_OVERLAP,
    BTD_GPT_SYSTEM_INDEX,BTD_GPT_SYSTEM_ALIGNMENT,BTD_GPT_DUPLICATE_SYSTEM,
    BTD_GPT_SYSTEM_MISSING
} BtdGptResult;
/* Non-identifying geometry only. Never contains GUIDs, names or raw CRC values. */
typedef struct {
    uint32_t capacity,header_size,entries,entry_size,entry_index,system_index,used_entries,problems;
    uint64_t primary_lba,backup_lba,first_usable,last_usable,table_lba,system_sectors;
    bool signature_valid;
    bool header_crc_checked,header_crc_ok,table_crc_checked,table_crc_ok;
} BtdGptInfo;
enum { BTD_GPT_SECTOR=1,BTD_GPT_SECTORS=33,BTD_GPT_BYTES=33*512 };
/* Fixed primary GPT profile. No partition identifier is copied or returned. */
bool btd_parse_gpt(const uint8_t *data,size_t size,uint32_t media_sectors,BtdWindow *out);
BtdGptResult btd_check_gpt(const uint8_t *data,size_t size,uint32_t media_sectors,
                         BtdWindow *out,BtdGptInfo *info);
const char *btd_gpt_result_stage(BtdGptResult result);
bool btd_read_allowed(const BtdWindow *window,uint32_t sector,uint32_t count);
/* Pure transport policy: write DMA and erase/program/lock commands are denied.
 * eMMC CMD6 accepts only bus/timing configuration and user-area selection. */
bool btd_command_allowed(bool emmc,uint32_t command,uint32_t argument,
                         bool has_request,bool is_write,uint32_t blocks,
                         uint32_t block_size,const BtdWindow *window);
uint32_t btd_crc32(const uint8_t *data,size_t size);
#endif
