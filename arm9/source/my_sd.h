#pragma once

#include <nds/disc_io.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool sdRemoved;
extern volatile bool sdWriteLocked;

void sdStatusHandler(u32 sdIrqStatus, void *userdata);

bool my_sdio_Shutdown();

const DISC_INTERFACE *__my_io_dsisd();

#ifdef __cplusplus
}
#endif
