/* SPDX-License-Identifier: GPL-2.0-only */
#include "diag_report.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
int main(void) {
    char number[11],expected[11];
    uint32_t values[]={0,1,9,10,99,100,2147483647u,2147483648u,UINT32_MAX};
    for(size_t i=0;i<sizeof(values)/sizeof(*values);i++) {
        snprintf(expected,sizeof(expected),"%u",values[i]);
        CHECK(!strcmp(btd_report_u32(values[i],number),expected));
    }
    BtdReport r={0};CHECK(btd_report_pages(&r)==1);
    CHECK(btd_report_add(&r,"KEYS",BTD_STATUS,-1,0));
    CHECK(btd_report_add(&r,"KEYS",BTD_STATUS,0,0));
    CHECK(btd_report_last(&r,"KEYS")->code==0);
    CHECK(btd_report_add(&r,"ENTRY_COUNT",BTD_NUMBER,0,24));CHECK(!btd_report_first_error(&r,NULL));
    CHECK(btd_report_add(&r,"CORE_READER",BTD_SKIPPED,0,0));CHECK(!btd_report_first_error(&r,NULL));
    CHECK(btd_report_add(&r,"HDR_B_CMAC",BTD_STATUS,5,0));size_t index=999;
    CHECK(btd_report_first_error(&r,&index)->code==5&&index==4);
    CHECK(!btd_report_last(&r,"ABSENT"));
    while(r.count<BTD_REPORT_MAX)CHECK(btd_report_add(&r,"stage",BTD_STATUS,0,0));
    CHECK(!btd_report_add(&r,"overflow",BTD_STATUS,5,0)&&r.dropped==1);
    CHECK(strcmp(btd_report_first_error(&r,NULL)->label,"HDR_B_CMAC")==0);
    size_t pages=btd_report_pages(&r);CHECK(pages==11);
    for(size_t p=0;p<pages;p++)CHECK(btd_report_move(btd_report_move(p,pages,1),pages,-1)==p);
    CHECK(btd_report_move(0,pages,-1)==pages-1&&btd_report_move(pages-1,pages,1)==0);
    CHECK(!btd_report_add(NULL,"x",BTD_STATUS,0,0));
    CHECK(!btd_report_add(&r,NULL,BTD_STATUS,0,0));CHECK(btd_report_move(0,0,1)==0);
    printf("Report: %u checks passed (root error, dependency skips, paging, overflow)\n",checks);return 0;
}
