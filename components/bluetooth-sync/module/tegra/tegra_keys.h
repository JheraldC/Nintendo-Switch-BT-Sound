/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTO_TEGRA_KEYS_H
#define BTO_TEGRA_KEYS_H
#include "tegra_platform.h"
#include "bt_keys.h"
/* SD must already be mounted. Fixed path, FA_READ, no fallback key dump. */
BtoKeyResult bto_tegra_keys_load(BtoKeys *out);
#endif
