#include "sha256.h"
#include <string.h>

void bsr_wipe(void *bytes, size_t size) {
    volatile uint8_t *p = bytes;
    while (size != 0) { *p++ = 0; --size; }
}
static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static void compress(BsrSha *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        const uint8_t *p = block + i * 4;
        w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }
    for (size_t i = 16; i < 64; ++i) {
        const uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        const uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=ctx->h[0],b=ctx->h[1],c=ctx->h[2],d=ctx->h[3];
    uint32_t e=ctx->h[4],f=ctx->h[5],g=ctx->h[6],h=ctx->h[7];
    for (size_t i=0; i<64; ++i) {
        const uint32_t t1=h+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        const uint32_t t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->h[0]+=a;ctx->h[1]+=b;ctx->h[2]+=c;ctx->h[3]+=d;
    ctx->h[4]+=e;ctx->h[5]+=f;ctx->h[6]+=g;ctx->h[7]+=h;
    bsr_wipe(w,sizeof(w));
}
void bsr_sha_init(BsrSha *ctx) {
    *ctx=(BsrSha){.h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
}
void bsr_sha_update(BsrSha *ctx, const void *bytes, size_t size) {
    const uint8_t *p=bytes;
    ctx->length += (uint64_t)size;
    while (size != 0) {
        size_t n=64-ctx->used;
        if (n>size) n=size;
        memcpy(ctx->block+ctx->used,p,n);
        ctx->used+=n;p+=n;size-=n;
        if (ctx->used==64) {compress(ctx,ctx->block);ctx->used=0;}
    }
}
void bsr_sha_final(BsrSha *ctx, uint8_t out[32]) {
    const uint64_t bits=ctx->length*8;
    ctx->block[ctx->used++]=0x80;
    if (ctx->used>56) {
        memset(ctx->block+ctx->used,0,64-ctx->used);compress(ctx,ctx->block);ctx->used=0;
    }
    memset(ctx->block+ctx->used,0,56-ctx->used);
    for (size_t i=0;i<8;++i) ctx->block[63-i]=(uint8_t)(bits>>(i*8));
    compress(ctx,ctx->block);
    for (size_t i=0;i<8;++i) for (size_t j=0;j<4;++j) out[i*4+j]=(uint8_t)(ctx->h[i]>>(24-j*8));
    bsr_wipe(ctx,sizeof(*ctx));
}
void bsr_sha256(const void *bytes, size_t size, uint8_t out[32]) {
    BsrSha ctx;bsr_sha_init(&ctx);bsr_sha_update(&ctx,bytes,size);bsr_sha_final(&ctx,out);
}
void bsr_hmac256(const uint8_t key[32], const void *bytes, size_t size, uint8_t out[32]) {
    uint8_t pad[64],inner[32];BsrSha ctx;
    memset(pad,0x36,sizeof(pad));for(size_t i=0;i<32;++i)pad[i]^=key[i];
    bsr_sha_init(&ctx);bsr_sha_update(&ctx,pad,64);bsr_sha_update(&ctx,bytes,size);bsr_sha_final(&ctx,inner);
    memset(pad,0x5c,sizeof(pad));for(size_t i=0;i<32;++i)pad[i]^=key[i];
    bsr_sha_init(&ctx);bsr_sha_update(&ctx,pad,64);bsr_sha_update(&ctx,inner,32);bsr_sha_final(&ctx,out);
    bsr_wipe(pad,sizeof(pad));bsr_wipe(inner,sizeof(inner));
}
