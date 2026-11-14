/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTA_CONTROLLER_PROFILES_H
#define BTA_CONTROLLER_PROFILES_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* Candidate identifiers from MissionControl's official controller headers.
 * This filter does not implement a driver, create a pairing, or add BLE support.
 * Legacy Pro/Joy-Con recognition remains in auto_sync.c and is always enabled. */
#ifndef BTA_EXTENDED_CONTROLLERS
#define BTA_EXTENDED_CONTROLLERS 0
#endif
static inline bool bta_missioncontrol_candidate(const uint8_t *r){
    if(!r||!memchr(r+0xD6,0,0xF9)||r[0x39]!=1||(r[0x27]&0x1F)!=5)return false;
    bool has_key=false;for(unsigned i=0;i<16;i++)if(r[0x29+i])has_key=true;
    if(!has_key)return false;
    uint16_t vid=(uint16_t)(r[0x40]|((uint16_t)r[0x41]<<8));
    uint16_t pid=(uint16_t)(r[0x42]|((uint16_t)r[0x43]<<8));
    const char *name=(const char *)r+0xD6;
    if(vid==0x054c){
        if(pid==0x05c4||pid==0x09cc)return !strcmp(name,"Wireless Controller");
        if(pid==0x0ce6)return !strcmp(name,"DualSense Wireless Controller")||!strcmp(name,"Wireless Controller");
        if(pid==0x0df2)return !strcmp(name,"DualSense Edge Wireless Controller");
    }
    if(vid==0x045e&&(pid==0x02e0||pid==0x02fd))return !strcmp(name,"Xbox Wireless Controller");
    return false;
}
#endif
