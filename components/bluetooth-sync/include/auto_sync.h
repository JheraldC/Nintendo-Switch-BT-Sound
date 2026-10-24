#ifndef BTA_AUTO_SYNC_H
#define BTA_AUTO_SYNC_H
#include "transfer.h"
enum { BTA_DB_MAX=8+BTH_MAX*512,BTA_STATE_MAX=96+BTH_MAX*40,BTA_TXN_SIZE=256 };
typedef struct {uint8_t address[6],controller,reserved,fingerprint[32];} BtaSeen;
typedef struct {uint32_t count;BtaSeen entries[BTH_MAX];} BtaState;
typedef struct {uint32_t added,updated,ignored;bool database_changed,history_changed;} BtaStats;
bool bta_controller(const uint8_t[512]);
bool bta_state_from_source(const BthRecords *,BtaState *);
bool bta_state_encode(const BtaState *,const uint8_t[32],uint8_t *,size_t,size_t *);
bool bta_state_decode(const uint8_t *,size_t,const uint8_t[32],BtaState *);
bool bta_database_encode(const BthRecords *,uint8_t *,size_t,size_t *);
bool bta_database_decode(const uint8_t *,size_t,BthRecords *);
bool bta_merge(const BthRecords *,const BthRecords *,const BtaState *,BthRecords *,BtaState *,BtaStats *);
enum BtaPath {BTA_DB,BTA_STATE,BTA_DB_NEXT,BTA_STATE_NEXT,BTA_DB_PREV,BTA_STATE_PREV,BTA_TXN,BTA_PATHS};
/* read: 1 success, 0 absent, -1 error. All callbacks are SD-only in the platform.
 * write creates a new file and must read back/verify before returning success. */
typedef struct {
    void *context;
    int (*read)(void *,enum BtaPath,uint8_t *,size_t,size_t *);
    bool (*write_new)(void *,enum BtaPath,const uint8_t *,size_t);
    bool (*rename)(void *,enum BtaPath,enum BtaPath);
    bool (*remove)(void *,enum BtaPath);
} BtaIo;
bool bta_recover(const BtaIo *,const uint8_t[32]);
bool bta_transaction(const BtaIo *,const uint8_t[32],const uint8_t *,size_t,const uint8_t *,size_t,const uint8_t *,size_t,const uint8_t *,size_t);
#endif
