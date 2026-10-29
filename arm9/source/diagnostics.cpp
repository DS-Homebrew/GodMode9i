#include "diagnostics.h"

#include "font.h"
#include "language.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <vector>

// firstCol / alignStart are app-wide RTL-aware layout globals (see ndsInfo.cpp),
// declared through the headers above.

static const char *wifiBoardName(u8 b) {
	switch (b) {
		case 1: return "DWM-W015";
		case 2: return "DWM-W024";
		case 3: return "DWM-W028";
		default: return "?";
	}
}

// Official DS firmware by build timestamp (018h) and part1/2 CRC16 (006h). FlashMe keeps the
// timestamp and saves the replaced firmware's CRC at 17Eh.
struct OfficialFirmware {
	u8 stamp[5];	// BCD minute, hour, day, month, year
	u16 partCrc;
	const char *name;
};
static const OfficialFirmware officialFirmware[] = {
	{{0x07, 0x11, 0x05, 0x10, 0x04}, 0x2C7A, "v1"},
	{{0x51, 0x09, 0x26, 0x11, 0x04}, 0xE0CE, "v2"},
	{{0x51, 0x08, 0x28, 0x02, 0x05}, 0xBFBA, "v3"},
	{{0x48, 0x14, 0x06, 0x06, 0x05}, 0xDFC7, "v4"},
	{{0x45, 0x14, 0x07, 0x12, 0x05}, 0x73B3, "v5"},
	{{0x33, 0x21, 0x05, 0x02, 0x06}, 0xE843, "v6 Lite"},
	{{0x19, 0x11, 0x08, 0x03, 0x06}, 0x0F1F, "v7 Lite"},
	{{0x15, 0x21, 0x09, 0x06, 0x05}, 0xF96D, "iQue v1"},
	{{0x35, 0x15, 0x26, 0x04, 0x06}, 0x87C4, "iQue v2 Lite"},
	{{0x30, 0x21, 0x09, 0x11, 0x06}, 0x74F0, "Kor v1 Lite"},
};

static const OfficialFirmware *findFirmwareByStamp(const u8 *stamp) {
	for (const auto &f : officialFirmware)
		if (memcmp(f.stamp, stamp, sizeof(f.stamp)) == 0)
			return &f;
	return nullptr;
}

static const OfficialFirmware *findFirmwareByCrc(u16 partCrc) {
	for (const auto &f : officialFirmware)
		if (f.partCrc == partCrc)
			return &f;
	return nullptr;
}

// Known DS firmware images by fingerprint (see arm7 main.c). A null flashMe is stock firmware.
struct FirmwareImage {
	u16 crc;
	bool tvOut;
	const char *flashMe;
};
static const FirmwareImage firmwareImages[] = {
	{0xBEFF, false, nullptr},	// v3 stock
	{0xDB95, false, nullptr},	// v5 stock
	{0x7965, false, nullptr},	// v6 stock
	{0xB5F0, false, nullptr},	// v7 stock
	{0x125B, false, nullptr},	// v1 stock
	{0x8322, false, nullptr},	// v2 stock
	{0x8FFB, false, nullptr},	// v4 stock
	{0xA247, false, nullptr},	// iQue v1 stock
	{0xE4B2, false, nullptr},	// iQue v2 stock
	{0x6130, false, nullptr},	// Kor v1 stock
	{0x803F, false, "v1"},
	{0x46E6, false, "v3"},
	{0xAC33, false, "v4"},
	{0x8487, false, "v4 stealth"},
	{0xC65F, false, "v5"},
	{0x43FD, false, "v5 stealth"},
	{0xBAAA, false, "v6"},
	{0xC202, false, "v6 stealth"},
	{0xF5D1, false, "v7 old"},
	{0x6030, false, "v7 old stealth"},
	{0x6BCC, false, "v7 new"},
	{0x407C, false, "v7 new stealth"},
	{0x13A7, false, "v7 new"},
	{0xD21C, false, "v7 new stealth"},
	{0xE0C0, false, "v8"},
	{0x7EFC, false, "v8 noauto"},
	{0x1639, false, "v8 stealth"},
	{0xE73C, false, "v8 stealth noauto"},
	{0x9FE4, false, "v8"},
	{0xE576, false, "v8 noauto"},
	{0x941A, false, "v8 stealth"},
	{0x3077, false, "v8 stealth noauto"},
	{0xD6D1, false, "v8a"},
	{0x3EAC, false, "v8a noauto"},
	{0x6DDF, false, "v8a stealth"},
	{0xA80F, false, "v8a stealth noauto"},
	{0x4FB9, false, "v8a"},
	{0x7FD0, false, "v8a noauto"},
	{0xCA73, false, "v8a stealth"},
	{0x672E, false, "v8a stealth noauto"},
	{0x8ABE, false, "v8a"},
	{0x37E2, false, "v8a noauto"},
	{0x3EB7, false, "v8a stealth"},
	{0x4E13, false, "v8a stealth noauto"},
	{0xFAC0, false, "v8a"},
	{0x0D6D, false, "v8a noauto"},
	{0x316B, false, "v8a stealth"},
	{0x4609, false, "v8a stealth noauto"},
	{0x53AA, true, "v8a"},
	{0xB2AA, true, "v8a noauto"},
	{0x1C2B, true, "v8a stealth"},
	{0x0F50, true, "v8a stealth noauto"},
	{0xF598, false, "v7 new"},
	{0x0FD7, false, "v7 new stealth"},
	{0x6BBA, false, "v7 new"},
	{0x5667, false, "v7 new stealth"},
	{0xA44F, false, "v7 new"},
	{0x48C9, false, "v7 new stealth"},
	{0x2C3C, false, "v8a"},
	{0x58FB, false, "v8a noauto"},
	{0xA232, false, "v8a stealth"},
	{0xABB7, false, "v8a stealth noauto"},
	{0xB204, false, "v8a"},
	{0x5B72, false, "v8a noauto"},
	{0x4DA2, false, "v8a stealth"},
	{0xC237, false, "v8a stealth noauto"},
	{0xF714, true, "v8a"},
	{0xB202, true, "v8a noauto"},
	{0x463D, true, "v8a stealth"},
	{0xA110, true, "v8a stealth noauto"},
	{0xE322, false, "v8a"},
	{0x29AF, false, "v8a noauto"},
	{0xFD39, false, "v8a stealth"},
	{0xFDF9, false, "v8a stealth noauto"},
	{0x280D, true, "v8a"},
	{0xC489, true, "v8a noauto"},
	{0x1730, true, "v8a stealth"},
	{0xB929, true, "v8a stealth noauto"},
};

static const FirmwareImage *findFirmwareImage(u16 crc) {
	for (const auto &f : firmwareImages)
		if (f.crc == crc)
			return &f;
	return nullptr;
}

// FlashMe build from the fingerprint, else a version group from FlashMe's own part1/2 CRC16.
static const char *flashMeName(const HardwareInfo &info, const FirmwareImage *image) {
	if (image)
		return image->flashMe ? image->flashMe : STR_DIAG_NO.c_str();
	if (!info.flashMeTail)
		return STR_DIAG_NO.c_str();
	switch (info.fwPartCrc) {
		case 0xABBB: return "v1-v4";
		case 0x82A7: return "v5-v8";
		case 0xA92D: return "v8a";
	}
	return STR_DIAG_UNKNOWN.c_str();
}

// JEP106 owner of the JEDEC manufacturer code, not necessarily the chip maker.
static const char *flashMaker(u32 jedecId) {
	switch (jedecId >> 16) {
		case 0x20: return "STMicro";
		case 0x62: return "Sanyo";
		case 0xC2: return "Macronix";
	}
	return nullptr;
}

static const char *regionName(u8 r) {
	static const char *const names[] = {"JPN", "USA", "EUR", "AUS", "CHN", "KOR"};
	return r < 6 ? names[r] : "?";
}

void diagnosticsShow(const HardwareInfo &info) {
	// Build the display lines. Each line is "<label>:" padded so the values align in a column.
	// The column width is computed from the longest label that can appear, so it holds up under
	// translation — a longer translated label just widens the column instead of breaking the
	// alignment. NO space before the colon. Optional fields are pushed only when their has* flag
	// is set — no "n/a" rows.
	std::vector<std::string> lines;
	char val[40];

	// Longest label (+1 for the colon) sets the value column. Starts at 8 for "JEDEC ID", the
	// widest English-literal label; the localized labels can grow past it in other languages.
	int labelW = 8;
	for (const std::string *s : { &STR_DIAG_CONSOLE_TYPE, &STR_DIAG_UNIT, &STR_DIAG_WIFI_BOARD,
	                              &STR_DIAG_RAM, &STR_DIAG_REGION, &STR_DIAG_FW_VERSION,
	                              &STR_DIAG_ORIGINAL_FW, &STR_DIAG_SERIAL, &STR_DIAG_CONSOLE_ID,
	                              &STR_DIAG_EMMC_CID, &STR_DIAG_WIFI_MAC })
		if ((int)s->length() > labelW) labelW = (int)s->length();
	labelW += 1;

	auto addLine = [&](const char *label, const char *value) {
		std::string line = label;
		if (*label)
			line += ':';
		if ((int)line.size() < labelW)
			line.append(labelW - line.size(), ' ');
		line += ' ';
		line += value;
		lines.push_back(line);
	};

	auto addGap = [&]() {
		if (!lines.empty() && !lines.back().empty())
			lines.push_back("");
	};

	// System group
	addLine(STR_DIAG_CONSOLE_TYPE.c_str(), info.consoleType.c_str());
	if (info.hasUnit)
		addLine(STR_DIAG_UNIT.c_str(), (info.unitRetail ? STR_DIAG_RETAIL : STR_DIAG_DEBUG).c_str());
	snprintf(val, sizeof(val), "%u MB", info.ramMB);
	addLine(STR_DIAG_RAM.c_str(), val);
	if (info.hasRegion)
		addLine(STR_DIAG_REGION.c_str(), regionName(info.region));
	if (info.hasScfgExt)	// bit31 = SCFG access (GBATEK: 0=Disable,1=Enable)
		addLine("SCFG", ((info.scfgExt & 0x80000000) ? STR_DIAG_ENABLED : STR_DIAG_DISABLED).c_str());

	// Firmware group, DS only. FW CRC is shown only for unknown firmware, so it can be reported.
	addGap();
	if (info.hasDsFirmware) {
		const FirmwareImage *image = findFirmwareImage(info.fwFingerprint);
		const OfficialFirmware *base = findFirmwareByStamp(info.fwStamp);
		if (base)
			snprintf(val, sizeof(val), "%s", base->name);
		else
			snprintf(val, sizeof(val), "20%02X-%02X-%02X", info.fwStamp[4], info.fwStamp[3], info.fwStamp[2]);
		addLine(STR_DIAG_FW_VERSION.c_str(), val);
		addLine("FlashMe", flashMeName(info, image));
		if (image && image->tvOut)
			addLine("TV-out", STR_DIAG_YES.c_str());
		// What FlashMe replaced, when it isn't the version FlashMe was built on.
		if (info.flashMeTail && info.flashMeOrigCrc != 0xFFFF) {
			const OfficialFirmware *orig = findFirmwareByCrc(info.flashMeOrigCrc);
			if (!orig)
				snprintf(val, sizeof(val), "%04X", info.flashMeOrigCrc);
			if (!orig || orig != base)
				addLine(STR_DIAG_ORIGINAL_FW.c_str(), orig ? orig->name : val);
		}
		if (!image) {
			snprintf(val, sizeof(val), "%04X", info.fwFingerprint);
			addLine("FW CRC", val);
		}
	}

	// Chips group
	addGap();
	snprintf(val, sizeof(val), "%04X", info.wifiChipId);
	addLine("W_ID", val);
	if (info.hasWifiBoard)
		addLine(STR_DIAG_WIFI_BOARD.c_str(), wifiBoardName(info.wifiBoard));
	const char *maker = flashMaker(info.jedecId);
	if (maker)
		snprintf(val, sizeof(val), "%06lX (%s)", (unsigned long)info.jedecId, maker);
	else
		snprintf(val, sizeof(val), "%06lX", (unsigned long)info.jedecId);
	addLine("JEDEC ID", val);

	// Identity group
	addGap();
	if (info.hasSerial)
		addLine(STR_DIAG_SERIAL.c_str(), info.serial);
	if (info.hasConsoleId) {
		snprintf(val, sizeof(val), "%02X%02X%02X%02X%02X%02X%02X%02X",
			info.consoleId[7], info.consoleId[6], info.consoleId[5], info.consoleId[4],
			info.consoleId[3], info.consoleId[2], info.consoleId[1], info.consoleId[0]);
		addLine(STR_DIAG_CONSOLE_ID.c_str(), val);
	}
	if (info.hasEmmcCid) {
		for (int i = 0; i < 16; i++)
			snprintf(val + i * 2, sizeof(val) - i * 2, "%02X", info.emmcCid[i]);
		std::string cid(val);
		addLine(STR_DIAG_EMMC_CID.c_str(), cid.substr(0, 16).c_str());
		addLine("", cid.c_str() + 16);
	}
	snprintf(val, sizeof(val), "%02X:%02X:%02X:%02X:%02X:%02X",
		info.mac[0], info.mac[1], info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
	addLine(STR_DIAG_WIFI_MAC.c_str(), val);

	// Visible data rows between the title row and the pinned footer.
	int totalRows = 192 / font->height();
	int visible = totalRows - 4;            // title, separator, spacer, footer
	if (visible < 1)
		visible = 1;
	int maxScroll = (int)lines.size() - visible;
	if (maxScroll < 0)
		maxScroll = 0;

	// Scrolling is kept implemented but currently dormant: every field fits on one screen,
	// so maxScroll is 0 and there is nothing to scroll. The Up/Down handling and the
	// "Up/Down: scroll" hint are gated on `scrollable` so they stay hidden while inert.
	// This is deliberately self-reactivating: if future fields push the list past one
	// screen, maxScroll becomes > 0, `scrollable` flips true, and scrolling comes back
	// with no further code changes needed.
	bool scrollable = maxScroll > 0;

	int scroll = 0;
	u16 pressed = 0, held = 0;
	while (1) {
		font->clear(false);
		font->printf(firstCol, 0, false, alignStart, Palette::white, "%s", STR_DIAG_TITLE.c_str());
		font->print(firstCol, 1, false, "----------------------------------------", alignStart);
		for (int i = 0; i < visible && (scroll + i) < (int)lines.size(); i++)
			font->print(firstCol, 3 + i, false, lines[scroll + i].c_str(), alignStart);
		// Only advertise scrolling when there is actually something to scroll.
		if (scrollable)
			font->printf(firstCol, -1, false, alignStart, Palette::white, "%s   %s",
				STR_DIAG_SCROLL_HINT.c_str(), STR_DIAG_CONTINUE.c_str());
		else
			font->printf(firstCol, -1, false, alignStart, Palette::white, "%s",
				STR_DIAG_CONTINUE.c_str());
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while (!held && !pressed);

		// Up/Down are inert while everything fits; they act only once `scrollable`.
		if (scrollable && (held & KEY_UP)) {
			if (scroll > 0)
				scroll--;
		} else if (scrollable && (held & KEY_DOWN)) {
			if (scroll < maxScroll)
				scroll++;
		} else if (pressed & KEY_A) {
			break;
		}
	}
}
