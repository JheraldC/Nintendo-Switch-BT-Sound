#include "auto_sync.h"
#include "controller_profiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
static void record(uint8_t *r,uint16_t vid,uint16_t pid,const char *name){
    memset(r,0,512);r[0]=2;r[5]=1;r[0x27]=0x25;r[0x29]=42;r[0x39]=1;
    r[0x40]=(uint8_t)vid;r[0x41]=(uint8_t)(vid>>8);r[0x42]=(uint8_t)pid;r[0x43]=(uint8_t)(pid>>8);
    strcpy((char *)r+0xD6,name);
}
int main(void){
    uint8_t r[512];
    const struct {uint16_t vid,pid;const char *name;} candidates[]={
        {0x054c,0x05c4,"Wireless Controller"},{0x054c,0x09cc,"Wireless Controller"},
        {0x054c,0x0ce6,"DualSense Wireless Controller"},{0x054c,0x0ce6,"Wireless Controller"},
        {0x054c,0x0df2,"DualSense Edge Wireless Controller"},
        {0x045e,0x02e0,"Xbox Wireless Controller"},{0x045e,0x02fd,"Xbox Wireless Controller"}};
    for(unsigned i=0;i<sizeof(candidates)/sizeof(*candidates);i++){
        record(r,candidates[i].vid,candidates[i].pid,candidates[i].name);
        CHECK(bta_missioncontrol_candidate(r));CHECK(bta_controller(r)==(BTA_EXTENDED_CONTROLLERS!=0));
        r[0x39]=0;CHECK(!bta_controller(r));r[0x39]=1;
        memset(r+0x29,0,16);CHECK(!bta_controller(r));r[0x29]=42;
        r[0x27]=0;CHECK(!bta_controller(r));r[0x27]=0x25;
        r[0x40]^=0x80;CHECK(!bta_controller(r));r[0x40]^=0x80;
        strcpy((char *)r+0xD6,"SYNTHETIC SPEAKER");CHECK(!bta_controller(r));
        memset(r+0xD6,'X',0xF9);CHECK(!bta_controller(r));
    }
    record(r,0x045e,0x0b13,"Xbox Wireless Controller");CHECK(!bta_controller(r)); /* BLE/Series PID is excluded. */
    record(r,0x1234,0x5678,"Wireless Controller");CHECK(!bta_controller(r));
    const char *legacy[]={"Pro Controller","Joy-Con (L)","Joy-Con (R)"};
    for(unsigned i=0;i<3;i++){record(r,0x2dc8,0x6001,legacy[i]);CHECK(bta_controller(r));}
    BthRecords source={0},destination={0},out={0};BtaState before={0},after={0};BtaStats stats;
    source.count=1;record(source.record[0],0x054c,0x09cc,"Wireless Controller");
    CHECK(bta_merge(&destination,&source,&before,&out,&after,&stats));
    CHECK(out.count==(BTA_EXTENDED_CONTROLLERS?1u:0u));
    CHECK(stats.added==(BTA_EXTENDED_CONTROLLERS?1u:0u));
    destination=out;before=after;
    CHECK(bta_merge(&destination,&source,&before,&out,&after,&stats));
    CHECK(!stats.database_changed&&!stats.history_changed);
    printf("Controller profiles: %u checks passed (legacy=%u, candidates=%u; synthetic records only)\n",checks,1u,(unsigned)BTA_EXTENDED_CONTROLLERS);
    return 0;
}
