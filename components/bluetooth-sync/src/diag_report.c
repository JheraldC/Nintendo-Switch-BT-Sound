/* SPDX-License-Identifier: GPL-2.0-only */
#include "diag_report.h"
#include <string.h>
const char *btd_report_u32(uint32_t value,char out[11]) {
    size_t n=0;do{out[n++]=(char)('0'+value%10);value/=10;}while(value);
    out[n]=0;
    for(size_t i=0;i<n/2;i++){char c=out[i];out[i]=out[n-1-i];out[n-1-i]=c;}
    return out;
}
bool btd_report_add(BtdReport *r,const char *label,BtdEventKind kind,int32_t code,uint32_t value) {
    if(!r||!label||kind>BTD_SKIPPED)return false;
    if(r->count>=BTD_REPORT_MAX){r->dropped++;return false;}
    BtdEvent *e=&r->events[r->count++];memset(e,0,sizeof(*e));
    size_t n=strlen(label);if(n>=sizeof(e->label))n=sizeof(e->label)-1;
    memcpy(e->label,label,n);e->kind=kind;e->code=code;e->value=value;return true;
}
const BtdEvent *btd_report_last(const BtdReport *r,const char *label) {
    if(!r||!label)return NULL;
    for(size_t i=r->count;i>0;i--)if(strcmp(r->events[i-1].label,label)==0)return &r->events[i-1];
    return NULL;
}
const BtdEvent *btd_report_first_error(const BtdReport *r,size_t *index) {
    if(!r)return NULL;
    for(size_t i=0;i<r->count;i++)if(r->events[i].kind==BTD_STATUS&&r->events[i].code>0) {
        if(index)*index=i;
        return &r->events[i];
    }
    return NULL;
}
size_t btd_report_pages(const BtdReport *r) { return 1+(r?(r->count+BTD_PAGE_ROWS-1)/BTD_PAGE_ROWS:0); }
size_t btd_report_move(size_t page,size_t pages,int dir) {
    if(!pages)return 0;
    if(page>=pages)page=0;
    if(dir>0)return page+1<pages?page+1:0;
    if(dir<0)return page?page-1:pages-1;
    return page;
}
