#pragma once

#include <nds/disc_io.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool sdRemoved;
extern volatile bool sdWriteLocked;

void sdStatusHandler(u32 sdIrqStatus, void *userdata);

#ifdef __cplusplus
}
#endif
