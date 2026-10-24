/* SPDX-License-Identifier: GPL-2.0-only
 * Explicit integration correction; all eight frozen source files remain intact.
 * Only the salt provider and diagnostic read observation are substituted.
 * Geometry, hash comparison, zero-sector semantics and return values are frozen.
 */
#include "internal.h"
#include "ivfc_bridge.h"
extern void btd_status(const char *,int);
extern void btd_number(const char *,uint32_t);
static BsrResult observed_read(View,uint64_t,void *,size_t);
#define bsr_hmac256 btd_ivfc_hmac
#define bsr_read observed_read
#define bsr_ivfc_init btd_frozen_ivfc_init
#include "../module/vendor/core/src/integrity.c"
#undef bsr_ivfc_init
#undef bsr_read
#undef bsr_hmac256
static Ivfc *observed[2];
static bool failure_reported;
BsrResult bsr_ivfc_init(BsrReader *r,Ivfc *ivfc,const uint8_t header[0xC0],unsigned levels,View master,View *data) {
    unsigned group=ivfc==&r->data_ivfc?0:1;
    if(group==0){observed[0]=NULL;observed[1]=NULL;failure_reported=false;}
    BsrResult result=btd_frozen_ivfc_init(r,ivfc,header,levels,master,data);
    if(result==BSR_OK)observed[group]=ivfc;
    return result;
}
static BsrResult observed_read(View view,uint64_t offset,void *out,size_t size) {
    BsrResult result=bsr_read(view,offset,out,size);
    if(result!=BSR_OK&&!failure_reported) {
        for(unsigned group=0;group<2;group++) {
            Ivfc *ivfc=observed[group];if(!ivfc)continue;
            for(unsigned i=0;i<ivfc->count&&i<4;i++)if(view.context==&ivfc->levels[i]) {
                failure_reported=true;
                btd_number("IVFC_FAILED_LEVEL",i+1);
                btd_number("IVFC_FAILED_BLOCK",(uint32_t)(offset/ivfc->levels[i].block_size));
                btd_status(group==0?"DATA_IVFC_HASH":"FAT_IVFC_HASH",(int)result);
                return result;
            }
        }
    }
    return result;
}
