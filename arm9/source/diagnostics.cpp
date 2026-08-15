#include "diagnostics.h"

#include "font.h"
#include "language.h"

#include <nds.h>
#include <stdio.h>
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
	char buf[64], val[40], lbl[40];

	// Longest label (+1 for the colon) sets the value column. Starts at 8 for "JEDEC ID", the
	// widest English-literal label; the localized labels can grow past it in other languages.
	int labelW = 8;
	for (const std::string *s : { &STR_DIAG_CONSOLE_TYPE, &STR_DIAG_UNIT, &STR_DIAG_WIFI_BOARD,
	                              &STR_DIAG_RAM, &STR_DIAG_REGION, &STR_DIAG_SERIAL,
	                              &STR_DIAG_CONSOLE_ID, &STR_DIAG_WIFI_MAC })
		if ((int)s->length() > labelW) labelW = (int)s->length();
	labelW += 1;

	auto addLine = [&](const char *label, const char *value) {
		snprintf(lbl, sizeof(lbl), "%s:", label);
		snprintf(buf, sizeof(buf), "%-*s %s", labelW, lbl, value);
		lines.push_back(buf);
	};

	// System group
	addLine(STR_DIAG_CONSOLE_TYPE.c_str(), info.consoleType.c_str());
	if (info.hasUnit)
		addLine(STR_DIAG_UNIT.c_str(), (info.unitRetail ? STR_DIAG_RETAIL : STR_DIAG_DEBUG).c_str());
	snprintf(val, sizeof(val), "%04X", info.wifiChipId);
	addLine("W_ID", val);
	if (info.hasWifiBoard)
		addLine(STR_DIAG_WIFI_BOARD.c_str(), wifiBoardName(info.wifiBoard));
	snprintf(val, sizeof(val), "%06lX", (unsigned long)info.jedecId);
	addLine("JEDEC ID", val);
	snprintf(val, sizeof(val), "%u MB", info.ramMB);
	addLine(STR_DIAG_RAM.c_str(), val);
	if (info.hasRegion)
		addLine(STR_DIAG_REGION.c_str(), regionName(info.region));
	if (info.hasScfgExt)	// bit31 = SCFG access (GBATEK: 0=Disable,1=Enable)
		addLine("SCFG", ((info.scfgExt & 0x80000000) ? STR_DIAG_ENABLED : STR_DIAG_DISABLED).c_str());

	// Identity fields (unique-ish IDs), no separator.
	if (info.hasSerial)
		addLine(STR_DIAG_SERIAL.c_str(), info.serial);
	if (info.hasConsoleId) {
		snprintf(val, sizeof(val), "%02X%02X%02X%02X%02X%02X%02X%02X",
			info.consoleId[0], info.consoleId[1], info.consoleId[2], info.consoleId[3],
			info.consoleId[4], info.consoleId[5], info.consoleId[6], info.consoleId[7]);
		addLine(STR_DIAG_CONSOLE_ID.c_str(), val);
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
