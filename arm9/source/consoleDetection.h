#pragma once

#include <stdbool.h>
#include <nds/ndstypes.h>

static inline bool consoleIsDSiFamily(bool dsiMode, u16 wifiChipId, u8 power5) {
	// Power management register 5: old DS mirrors register 1 at 5 (excluded by W_ID 1440h),
	// DS Lite mirrors register 4 at 5 (never 00h), DSi/3DS read 00h.
	return dsiMode || (wifiChipId != 0x1440 && power5 == 0);
}

static inline bool consoleIsCpu20(u16 wifiChipId, u8 power4) {
	// Late DS Phat: original WiFi chip with a DS Lite power chip, whose register 4 bits 4-7 read 4.
	// On the original power chip register 4 mirrors register 0, where bit6 means shutdown.
	return wifiChipId == 0x1440 && (power4 & 0xF0) == 0x40;
}

static inline bool consoleHas3DSFirmware(u8 wifiVersion) {
	// Firmware 02Fh: 15 = W015, 24 = W024 (DSi). Anything else in the DSi family is a 3DS.
	return wifiVersion != 15 && wifiVersion != 24;
}
