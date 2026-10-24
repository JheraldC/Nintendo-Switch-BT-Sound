/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_REPORT_H
#define BTD_REPORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { BTD_REPORT_MAX=160,BTD_PAGE_ROWS=16,BTD_LABEL_SIZE=44 };
typedef enum { BTD_STATUS=0,BTD_NUMBER,BTD_SKIPPED } BtdEventKind;
typedef struct { char label[BTD_LABEL_SIZE];BtdEventKind kind;int32_t code;uint32_t value; } BtdEvent;
typedef struct { BtdEvent events[BTD_REPORT_MAX];size_t count,dropped; } BtdReport;
bool btd_report_add(BtdReport *,const char *,BtdEventKind,int32_t,uint32_t);
const BtdEvent *btd_report_last(const BtdReport *,const char *);
const BtdEvent *btd_report_first_error(const BtdReport *,size_t *index);
size_t btd_report_pages(const BtdReport *);
size_t btd_report_move(size_t page,size_t pages,int direction);
const char *btd_report_u32(uint32_t value,char out[11]);
#endif
