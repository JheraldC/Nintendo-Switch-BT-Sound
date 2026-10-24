#ifndef BTX_ORIGIN_PACKET_H
#define BTX_ORIGIN_PACKET_H
#include "transfer.h"
enum { BTX_SECTORS=25,BTX_BYTES=BTX_SECTORS*512 };
typedef struct {
    uint32_t sectors[BTX_SECTORS];
    uint32_t current;
    const uint8_t *buffer;
    bool ready,armed;
} BtxWindow;
bool btx_map_init(BtxWindow *,const uint32_t[BTX_SECTORS],uint32_t,uint32_t);
bool btx_arm(BtxWindow *,uint32_t,const uint8_t *);
void btx_disarm(BtxWindow *);
bool btx_command(const BtxWindow *,bool,uint32_t,uint32_t,bool,uint32_t,uint32_t);
bool btx_packet_build(const BthRecords *,const uint8_t[32],const uint8_t[32],uint8_t[BTX_BYTES]);
bool btx_packet_parse(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],BthRecords *);
#endif
