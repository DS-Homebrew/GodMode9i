#ifndef PXI_H
#define PXI_H

#include "pxiVars.h"

#include <calico/system/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Thread s_pxiThread;
extern u8 s_pxiThreadStack[1024];

int pxiThreadMain(void* arg);

#ifdef __cplusplus
}
#endif

#endif // PXI_H
