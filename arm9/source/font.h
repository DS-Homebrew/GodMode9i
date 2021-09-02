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
	constexpr static char16_t arabicPresentationForms[][3] = {
		// Initial, Medial, Final
		{u'آ', u'ﺂ', u'ﺂ'}, // Alef with madda above
		{u'أ', u'ﺄ', u'ﺄ'}, // Alef with hamza above
		{u'ؤ', u'ﺆ', u'ﺆ'}, // Waw with hamza above
		{u'إ', u'ﺈ', u'ﺈ'}, // Alef with hamza below
		{u'ﺋ', u'ﺌ', u'ﺊ'}, // Yeh with hamza above
		{u'ا', u'ﺎ', u'ﺎ'}, // Alef
		{u'ﺑ', u'ﺒ', u'ﺐ'}, // Beh
		{u'ة', u'ﺔ', u'ﺔ'}, // Teh marbuta
		{u'ﺗ', u'ﺘ', u'ﺖ'}, // Teh
		{u'ﺛ', u'ﺜ', u'ﺚ'}, // Theh
		{u'ﺟ', u'ﺠ', u'ﺞ'}, // Jeem
		{u'ﺣ', u'ﺤ', u'ﺢ'}, // Hah
		{u'ﺧ', u'ﺨ', u'ﺦ'}, // Khah
		{u'د', u'ﺪ', u'ﺪ'}, // Dal
		{u'ذ', u'ﺬ', u'ﺬ'}, // Thal
		{u'ر', u'ﺮ', u'ﺮ'}, // Reh
		{u'ز', u'ﺰ', u'ﺰ'}, // Zain
		{u'ﺳ', u'ﺴ', u'ﺲ'}, // Seen
		{u'ﺷ', u'ﺸ', u'ﺶ'}, // Sheen
		{u'ﺻ', u'ﺼ', u'ﺺ'}, // Sad
		{u'ﺿ', u'ﻀ', u'ﺾ'}, // Dad
		{u'ﻃ', u'ﻄ', u'ﻂ'}, // Tah
		{u'ﻇ', u'ﻈ', u'ﻆ'}, // Zah
		{u'ﻋ', u'ﻌ', u'ﻊ'}, // Ain
		{u'ﻏ', u'ﻐ', u'ﻎ'}, // Ghain
		{u'ػ', u'ػ', u'ػ'}, // Keheh with two dots above
		{u'ؼ', u'ؼ', u'ؼ'}, // Keheh with three dots below
		{u'ؽ', u'ؽ', u'ؽ'}, // Farsi yeh with inverted v
		{u'ؾ', u'ؾ', u'ؾ'}, // Farsi yeh with two dots above
		{u'ؿ', u'ؿ', u'ؿ'}, // Farsi yeh with three docs above
		{u'ـ', u'ـ', u'ـ'}, // Tatweel
		{u'ﻓ', u'ﻔ', u'ﻒ'}, // Feh
		{u'ﻗ', u'ﻘ', u'ﻖ'}, // Qaf
		{u'ﻛ', u'ﻜ', u'ﻚ'}, // Kaf
		{u'ﻟ', u'ﻠ', u'ﻞ'}, // Lam
		{u'ﻣ', u'ﻤ', u'ﻢ'}, // Meem
		{u'ﻧ', u'ﻨ', u'ﻦ'}, // Noon
		{u'ﻫ', u'ﻬ', u'ﻪ'}, // Heh
		{u'و', u'ﻮ', u'ﻮ'}, // Waw
		{u'ﯨ', u'ﯩ', u'ﻰ'}, // Alef maksura
		{u'ﻳ', u'ﻴ', u'ﻲ'}, // Yeh
	};

	static bool isArabic(char16_t c);
	static bool isStrongRTL(char16_t c);
	static bool isWeak(char16_t c);
	static bool isNumber(char16_t c);

	static char16_t arabicForm(char16_t current, char16_t prev, char16_t next);

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

	bool load(const char *path);

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