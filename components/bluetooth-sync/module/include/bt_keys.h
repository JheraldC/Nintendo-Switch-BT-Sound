/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTO_KEYS_H
#define BTO_KEYS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Only the two console-specific keys needed for SYSTEM and save authentication.
 * No key is logged, persisted, or derived by this component. */
typedef struct { uint8_t bis_system[32]; uint8_t save_mac[16]; } BtoKeys;
typedef bool (*BtoKeyRead)(void *context,void *out,size_t count);
typedef enum { BTO_KEYS_OK=0,BTO_KEYS_ARGUMENT,BTO_KEYS_IO,
    BTO_KEYS_FORMAT,BTO_KEYS_MISSING,BTO_KEYS_DUPLICATE } BtoKeyResult;
enum { BTO_KEYS_FILE_MAX=256*1024 };
/* Sequential callback must deliver exactly count bytes or return false.
 * Ignores unrelated lines, including long values, without decoding their keys.
 * On any failure output and temporary buffers are cleared. */
BtoKeyResult bto_keys_parse(BtoKeyRead read,void *context,size_t size,BtoKeys *out);
#endif
