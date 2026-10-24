#ifndef HOST_NDS_H
#define HOST_NDS_H

#include <nds/ndstypes.h>

constexpr u32 KEY_A = 1 << 0;
constexpr u32 KEY_UP = 1 << 6;
constexpr u32 KEY_DOWN = 1 << 7;

void swiWaitForVBlank();
void scanKeys();
u32 keysDown();
u32 keysDownRepeat();

#endif
