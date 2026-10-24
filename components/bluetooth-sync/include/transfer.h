#ifndef BTH_TRANSFER_H
#define BTH_TRANSFER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { BTH_RECORD=512,BTH_MAX=24,BTH_HEADER=64,BTH_PACKET_MAX=96+24*512 };
typedef void (*BthHash)(const void *,size_t,uint8_t[32]);
typedef struct { size_t count;uint8_t record[BTH_MAX][BTH_RECORD]; } BthRecords;
void bth_wipe(void *,size_t);
bool bth_records_valid(const BthRecords *);
bool bth_packet_build(const BthRecords *,uint32_t,const uint8_t[32],BthHash,uint8_t *,size_t,size_t *);
bool bth_packet_parse(const uint8_t *,size_t,uint32_t,const uint8_t[32],BthHash,BthRecords *);
/* Explicit source slot selection works for generic controllers too. Never evict. */
bool bth_merge_selected(const BthRecords *,const BthRecords *,const size_t *,size_t,BthRecords *);
#endif
