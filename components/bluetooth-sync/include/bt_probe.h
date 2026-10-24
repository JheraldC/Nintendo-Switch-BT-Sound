/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_BT_PROBE_H
#define BTD_BT_PROBE_H
#include "bt_save_reader.h"
typedef enum {
    BTD_BT_SIZE=1, BTD_BT_HEADER, BTD_BT_PROFILE, BTD_BT_COUNT,
    BTD_BT_READ, BTD_BT_RECORD, BTD_BT_DUPLICATE, BTD_BT_KEY, BTD_BT_DONE
} BtdBtStage;
/* Only non-identifying counters survive. No raw pairing record leaves this API. */
typedef struct {
    uint32_t stage,version,flags,count,validated,joy_l,joy_r,pro,other;
    uint32_t bytes_read,failed_slot,layout_known,complete;
} BtdBtSummary;
BsrResult btd_bt_probe(BsrReader *, BtdBtSummary *);
#include "transfer.h"
BsrResult btd_bt_collect(BsrReader *,BtdBtSummary *,BthRecords *);
#endif
