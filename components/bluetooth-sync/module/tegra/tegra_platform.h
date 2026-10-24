/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTO_TEGRA_PLATFORM_H
#define BTO_TEGRA_PLATFORM_H
#ifdef BTO_HOST_TEST
#include "mock_tegra.h"
#else
/* BDK uses typedef int bool. Parse every platform type before the portable
 * reader introduces C99 stdbool, preserving the original BDK structure ABI. */
#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>
#include <storage/sdmmc.h>
#include <sec/se.h>
#include "source/storage/mountmanager.h"
#endif
#endif
