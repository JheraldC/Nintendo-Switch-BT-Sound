/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTO_TEGRA_READER_H
#define BTO_TEGRA_READER_H
#include "tegra_platform.h"
#include "bt_save_reader.h"
typedef struct {
    FIL file;
    BsrReader *reader;
    bool file_open;
    bool storage_connected;
    bool owns_bis_slots;
} BtoTegraReader;
/* Expects a console-specific save-MAC key supplied in RAM by an audited provider.
 * Low-level entry point: this function does not load keys or read BOOT0.
 * Source layout extraction is deliberately separate: no production offsets
 * for the internal settings /file have been established for HOS 23.0.1. */
BsrResult bto_tegra_open(BtoTegraReader *context,const uint8_t save_mac_key[16],BsrError *error);
/* Complete minimal key path: read the existing prod.keys from SD, load only
 * SYSTEM BIS slots 4/5, pass MAC key in RAM, then wipe the temporary key copy.
 * Close clears slots 4/5. A fresh, zero-initialized context is required.
 * No key extraction fallback, no general TegraExplorer SetKeySlots call. */
BsrResult bto_tegra_open_from_sd(BtoTegraReader *context,BsrError *error);
void bto_tegra_close(BtoTegraReader *context);
#endif
