/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BT_ONLY_H
#define BT_ONLY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Atmosphere 1.12.1 BluetoothDevicesSettings, not SYSTEM/save file offsets. */
enum { BTO_RECORD_SIZE=0x200, BTO_MAX_RECORDS=20,
       BTO_DATABASE_MAX=8+BTO_MAX_RECORDS*BTO_RECORD_SIZE };
typedef enum {
    BTO_OK=0, BTO_ARGUMENT, BTO_FORMAT, BTO_COUNT, BTO_DUPLICATE,
    BTO_NOT_FOUND, BTO_NOT_NINTENDO, BTO_NO_LINK_KEY, BTO_CAPACITY
} BtoResult;
typedef struct { uint8_t bytes[BTO_RECORD_SIZE]; } BtoRecord;
typedef enum { BTO_NINTENDO_NONE=0,BTO_NINTENDO_JOY_L,BTO_NINTENDO_JOY_R,BTO_NINTENDO_PRO } BtoNintendoType;
/* Validate one modern record; return only a generic controller class. */
BtoResult bto_nintendo_type(const BtoRecord *,BtoNintendoType *);
typedef struct { size_t count; BtoRecord records[BTO_MAX_RECORDS]; } BtoDatabase;
void bto_wipe(void *p,size_t n);
/* These functions neither access storage nor log any pairing data. */
BtoResult bto_validate(const BtoDatabase *db);
BtoResult bto_decode(const uint8_t *data,size_t size,BtoDatabase *out);
BtoResult bto_encode(const BtoDatabase *db,uint8_t *out,size_t capacity,size_t *written);
/* Exact address selection: unrelated controllers are never selected implicitly. */
BtoResult bto_select_nintendo(const BtoDatabase *source,
    const uint8_t (*addresses)[6],size_t address_count,BtoDatabase *out);
/* Keep other destination pairings; reject overflow instead of evicting them. */
BtoResult bto_merge(const BtoDatabase *destination,const BtoDatabase *selected,
                    BtoDatabase *out);
const char *bto_result_name(BtoResult result);
#endif
