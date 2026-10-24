#include "diagnostics.h"
#include "font.h"
#include "language.h"

#include <nds.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#define STRING(what, def) std::string STR_##what = def;
#include "language.inl"
#undef STRING

int firstCol = 1;
Alignment alignStart = Alignment::left;
static u8 fontWidth = 6;
static int scans = 0;
static std::vector<std::string> rendered;
Font *font = nullptr;

u8 Font::textBuf[2][256 * 192] = {};
bool Font::mainScreen = false;

Font::Font(const char *) {
	tileWidth = fontWidth;
	tileHeight = 8;
}

Font::~Font() {}

std::u16string Font::utf8to16(std::string_view text) {
	return std::u16string(text.begin(), text.end());
}

void Font::print(int, int, bool, std::u16string_view text, Alignment, Palette, bool, bool) {
	rendered.emplace_back(text.begin(), text.end());
}

void Font::printf(int, int, bool, Alignment, Palette, const char *, ...) {}

extern "C" void tonccpy(void *, const void *, unsigned int) {}

void swiWaitForVBlank() {}
void scanKeys() { scans++; }
u32 keysDown() { return scans >= 32 ? KEY_A : 0; }
u32 keysDownRepeat() { return scans < 32 ? KEY_DOWN : 0; }

static void show(const HardwareInfo &info, u8 width = 6) {
	fontWidth = width;
	Font testFont(nullptr);
	font = &testFont;
	scans = 0;
	rendered.clear();
	diagnosticsShow(info);
	font = nullptr;
}

static bool contains(const std::string &text) {
	return std::find(rendered.begin(), rendered.end(), text) != rendered.end();
}

static bool hasConsoleId(const char *value) {
	return std::any_of(rendered.begin(), rendered.end(), [&](const std::string &line) {
		return line.find("Console ID:") == 0 && line.find(value) != std::string::npos;
	});
}

static void expectCid(const char *first, const char *second) {
	auto line = std::find(rendered.begin(), rendered.end(),
		std::string("eMMC CID:     ") + first);
	assert(line != rendered.end());
	assert(line + 1 != rendered.end());
	assert(*(line + 1) == std::string(14, ' ') + second);
}

int main() {
	HardwareInfo info = {};
	info.consoleType = "Nintendo DSi";
	info.hasConsoleId = true;
	const u8 consoleId[] = {0x23, 0x01, 0x89, 0x67, 0x45, 0x23, 0xA1, 0x08};
	std::memcpy(info.consoleId, consoleId, sizeof(consoleId));
	info.hasEmmcCid = true;
	const u8 cid[] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
		0x10, 0x32, 0x54, 0x76, 0x00, 0x00, 0x15, 0x00
	};
	std::memcpy(info.emmcCid, cid, sizeof(cid));

	show(info);
	assert(hasConsoleId("08A1234567890123"));
	expectCid("0123456789ABCDEF", "1032547600001500");
	assert(std::memcmp(info.consoleId, consoleId, sizeof(consoleId)) == 0);
	assert(std::memcmp(info.emmcCid, cid, sizeof(cid)) == 0);

	show(info, 8);
	expectCid("0123456789ABCDEF", "1032547600001500");
	assert(!contains("0123456789ABCDEF1032547600001500"));

	std::memset(info.consoleId, 0, sizeof(info.consoleId));
	std::memset(info.emmcCid, 0, sizeof(info.emmcCid));
	show(info);
	assert(hasConsoleId("0000000000000000"));
	expectCid("0000000000000000", "0000000000000000");

	info.hasConsoleId = false;
	info.hasEmmcCid = false;
	show(info);
	assert(std::none_of(rendered.begin(), rendered.end(), [](const std::string &line) {
		return line.find("eMMC CID:") == 0;
	}));
	assert(!hasConsoleId("0000000000000000"));
	std::puts("Hardware info tests passed");
}
