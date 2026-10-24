#ifndef BTX_SD_EXPORT_H
#define BTX_SD_EXPORT_H
#include "transfer.h"
/* Called only after authenticated source reading and eMMC disconnection. */
int btx_export_sd(const BthRecords *);
/* Before-DMA guard: the window is normally closed and never applies to eMMC. */
int btx_transport_allowed(unsigned,unsigned,int,unsigned,unsigned);
#endif
