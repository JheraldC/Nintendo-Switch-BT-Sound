/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_IVFC_BRIDGE_H
#define BTD_IVFC_BRIDGE_H
#include <stdint.h>
#include <stddef.h>
/* Adapter for the frozen caller's (seed,label) argument order.
   Format definition: HMAC-SHA256(KEY=label, MESSAGE=seed[32]). */
void btd_ivfc_hmac(const uint8_t seed[32],const void *label,size_t label_size,uint8_t out[32]);
#endif
