#ifndef BTA_AUTO_PLATFORM_H
#define BTA_AUTO_PLATFORM_H
#include "auto_sync.h"
const BtaIo *bta_platform_io(void);
int bta_platform_guards(void);
int bta_sd_disk_write(const void *,unsigned,unsigned);
int bta_sd_sync(void);
int btd_nand_connected(void);
int bta_prepare_boot(void);
int bta_continue_boot(int);
#endif
