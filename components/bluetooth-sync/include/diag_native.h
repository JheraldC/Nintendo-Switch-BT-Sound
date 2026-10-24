/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BTD_DIAG_NATIVE_H
#define BTD_DIAG_NATIVE_H
/* Include BDK before portable bool definitions: its structures use int bool. */
#if defined(BTA_PLATFORM_HOST_TEST)
#include "mock_auto_native.h"
#elif defined(BTD_HOST_TEST)
#include "mock_native.h"
#else
#include "source/storage/mountmanager.h"
#include <storage/sdmmc_driver.h>
#endif
int btd_transport_guard(const sdmmc_t *controller,const sdmmc_cmd_t *command,const sdmmc_req_t *request);
int btd_validate_ext_csd(const void *data);
void btd_status(const char *stage,int result);
void btd_number(const char *label,u32 value);
void btd_skip(const char *label);
void btd_storage_summary(void);
#endif
