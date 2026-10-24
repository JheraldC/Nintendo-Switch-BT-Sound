#ifndef BSR_SHA256_H
#define BSR_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t h[8]; uint64_t length; uint8_t block[64]; size_t used; } BsrSha;
void bsr_sha_init(BsrSha *ctx);
void bsr_sha_update(BsrSha *ctx, const void *bytes, size_t size);
void bsr_sha_final(BsrSha *ctx, uint8_t out[32]);
void bsr_sha256(const void *bytes, size_t size, uint8_t out[32]);
void bsr_hmac256(const uint8_t key[32], const void *bytes, size_t size, uint8_t out[32]);
void bsr_wipe(void *bytes, size_t size);
#endif
