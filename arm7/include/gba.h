#define GBA_H
#ifdef GBA_H

#include <nds/ndstypes.h>

bool gbaReadEeprom(u8 *dst, u32 src, u32 len);
bool gbaWriteEeprom(u32 dst, u8 *src, u32 len);

#endif // GBA_H
