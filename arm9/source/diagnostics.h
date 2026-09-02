#pragma once

#include <nds/ndstypes.h>
#include <string>

// Hardware/debug parameters for the hidden hardware-info screen. Optional (DSi-only or
// value-validated) fields carry a `has*` flag; the screen omits a field's line entirely when
// its flag is false — there are no "n/a" rows.
struct HardwareInfo {
	// Always present
	std::string consoleType;   // GBATEK 01Dh spelling, e.g. "Nintendo DS-lite"
	u16 wifiChipId;            // W_ID (4808000h): 1440h=DS, C340h=DS Lite
	u32 jedecId;               // firmware flash JEDEC ID (RDID), big-endian packed
	u8 ramMB;                  // 4 / 16 / 32
	u8 mac[6];                 // WiFi MAC (firmware 036h)

	// Optional (omitted when the flag is false)
	bool hasUnit;              // SCFG_OP available (DSi)
	bool unitRetail;           // true=retail, false=debug
	bool hasWifiBoard;         // firmware 1FDh holds a known board id
	u8 wifiBoard;              // 1=DWM-W015, 2=W024, 3=W028
	bool hasRegion;
	u8 region;                 // 0=JPN..5=KOR
	bool hasScfgExt;           // SCFG present (DSi)
	u32 scfgExt;
	bool hasSerial;
	char serial[13];           // up to 12 ASCII + NUL
	bool hasConsoleId;
	u8 consoleId[8];
};

// Shows the hardware-info screen and blocks until the user presses B.
void diagnosticsShow(const HardwareInfo &info);
