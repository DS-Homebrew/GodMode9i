#ifndef FONT_H
#define FONT_H

#include "tonccpy.h"

#include <nds/arm9/background.h>
#include <nds/dma.h>
#include <nds/ndstypes.h>
#include <string>

#define TILE_MAX_WIDTH 8
#define TILE_MAX_HEIGHT 10

#define SCREEN_COLS (256 / font->width())
#define ENTRIES_PER_SCREEN ((192 - font->height()) / font->height())

enum class Alignment {
	left,
	center,
	right,
};

enum class Palette : u8 {
	white = 0,
	gray,
	red,
	green,
	greenAlt,
	blue,
	yellow,
	blackRed,
	blackGreen,
	blackBlue,
};

class Font {
	static bool isStrongRTL(char16_t c);
	static bool isWeak(char16_t c);
	static bool isNumber(char16_t c);

	static constexpr u16 palette[16][2] = {
		{0x0000, 0x7FFF}, // White
		{0x0000, 0x3DEF}, // Gray
		{0x0000, 0x001F}, // Red
		{0x0000, 0x03E0}, // Green
		{0x0000, 0x02E0}, // Green (alt)
		{0x0000, 0x656A}, // Blue
		{0x0000, 0x3339}, // Yellow
		{0x001F, 0x0000}, // Black on red
		{0x03E0, 0x0000}, // Black on green
		{0x656A, 0x0000}, // Black on blue
	};

	static u8 textBuf[2][256 * 192];
	static bool mainScreen;

	u8 tileWidth = 0, tileHeight = 0;
	u16 tileCount = 0;
	u16 questionMark = 0;
	u8 *fontTiles = nullptr;
	u16 *fontMap = nullptr;

	u16 getCharIndex(char16_t c);
public:
	static std::u16string utf8to16(std::string_view text);

	static void update(bool top) { tonccpy(bgGetGfxPtr(top ? 2 : 6), Font::textBuf[top ^ mainScreen], 256 * 192); }
	static void clear(bool top) { dmaFillWords(0, Font::textBuf[top ^ mainScreen], 256 * 192); }

	static void mainOnTop(bool top) { mainScreen = !top; }

	Font(const char *path);

	~Font(void);

	u8 width(void) { return tileWidth; }
	u8 height(void) { return tileHeight; }

	int calcWidth(std::string_view text) { return utf8to16(text).length(); }
	int calcWidth(std::u16string_view text) { return text.length(); };

	int calcHeight(std::string_view text, int xPos = 0) { return calcHeight(utf8to16(text)); }
	int calcHeight(std::u16string_view text, int xPos = 0);

	void printf(int xPos, int yPos, bool top, Alignment align, Palette palette, const char *format, ...);

	void print(int xPos, int yPos, bool top, int value, Alignment align = Alignment::left, Palette palette = Palette::white) { print(xPos, yPos, top, std::to_string(value), align, palette); }
	void print(int xPos, int yPos, bool top, std::string_view text, Alignment align = Alignment::left, Palette palette = Palette::white) { print(xPos, yPos, top, utf8to16(text), align, palette); }
	void print(int xPos, int yPos, bool top, std::u16string_view text, Alignment align = Alignment::left, Palette palette = Palette::white, bool rtl = false);
};

extern Font *font;

#endif // FONT_H