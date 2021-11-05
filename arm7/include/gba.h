#define GBA_H
#ifdef GBA_H

#include <nds/ndstypes.h>

void readEeprom(u8 *dst, u32 src, u32 len);
void writeEeprom(u32 dst, u8 *src, u32 len);

#endif // GBA_H
