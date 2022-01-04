#pragma once

#include <nds.h>
#include <nds/disc_io.h>

#ifdef __cplusplus
extern "C" {
#endif

bool img_shutdown();

extern const DISC_INTERFACE io_img;
extern const DISC_INTERFACE io_dsiware_save;

#ifdef __cplusplus
}
#endif
