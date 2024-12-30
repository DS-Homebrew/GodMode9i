#pragma once

#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/disc_io.h>

extern u32 ramdSectors;
extern u8* ramdLocMep;

#ifdef __cplusplus
extern "C" {
#endif

extern void ramd_setSize(const bool ram32MB);

#ifdef __cplusplus
}
#endif

extern const DISC_INTERFACE io_ram_drive;
