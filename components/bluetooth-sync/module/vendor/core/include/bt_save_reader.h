#ifndef BT_SAVE_READER_H
#define BT_SAVE_READER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BSR_OK = 0, BSR_ARGUMENT, BSR_RANGE, BSR_IO, BSR_FORMAT,
    BSR_AUTH, BSR_INTEGRITY, BSR_LIMIT, BSR_MEMORY, BSR_CYCLE,
    BSR_NOT_FOUND, BSR_AMBIGUOUS, BSR_UNSUPPORTED
} BsrResult;

/* Return true ONLY when all requested bytes have been delivered.
 * Partial reads must return false. Source must stay immutable for reader lifetime.
 * No write/repair/flush/commit/export capability exists in this interface. */
typedef struct {
    void *context;
    bool (*ReadExact)(void *context, uint64_t offset, void *buffer, size_t size);
    bool (*GetSize)(void *context, uint64_t *size);
} BsrStorage;

/* Mandatory trusted authentication provider. A false result is fatal.
 * Each header is exactly 0x4000 bytes. The provider must authenticate its
 * 16-byte CMAC against the DISF region [0x100,0x300), not merely check a marker.
 * This core neither receives nor derives console keys. */
typedef bool (*BsrValidateCmac)(void *context, const uint8_t *header, size_t size);
typedef struct BsrReader BsrReader;
typedef struct { BsrResult result; const char *stage; } BsrError;

BsrResult bsr_open(const BsrStorage *storage, BsrValidateCmac validate_cmac,
                   void *cmac_context, BsrReader **out, BsrError *error);
/* Lookup is exclusively /file; no general path traversal or export function. */
BsrResult bsr_file_size(const BsrReader *reader, uint64_t *size);
BsrResult bsr_file_read(BsrReader *reader, uint64_t offset, void *buffer, size_t size);
/* Invalidates the pointer and wipes all reader-owned allocations before freeing. */
void bsr_close(BsrReader **reader);
const char *bsr_result_string(BsrResult result);
#endif
