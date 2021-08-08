#include "font.h"

#include "font_default_frf.h"
#include "tonccpy.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include<nds.h>
u8 Font::textBuf[2][256 * 192];
bool Font::mainScreen = false;

Font *font = nullptr;

bool Font::isStrongRTL(char16_t c) {
	return (c >= 0x0590 && c <= 0x05FF) || c == 0x200F;
}

bool Font::isWeak(char16_t c) {
	return c < 'A' || (c > 'Z' && c < 'a') || (c > 'z' && c < 127);
}

bool Font::isNumber(char16_t c) {
	return c >= '0' && c <= '9';
}

Font::Font(const char *path) {
	FILE *file = fopen(path, "rb");
	
	const u8 *fileBuffer = font_default_frf;
	if(file) {
		fseek(file, 0, SEEK_END);
		size_t size = ftell(file);

		fileBuffer = new u8[size];
		if(!fileBuffer) {
			fclose(file);
			return;
		}

		fseek(file, 0, SEEK_SET);
		fread((void *)fileBuffer, 1, size, file);
	}
	const u8 *ptr = fileBuffer;

	// Check header magic, then skip over
	if(memcmp(ptr, "RIFF", 4) != 0)
		goto cleanup;

	ptr += 8;

	// check for and load META section
	if(memcmp(ptr, "META", 4) == 0) {
		tileWidth = ptr[8];
		tileHeight = ptr[9];
		tonccpy(&tileCount, ptr + 10, sizeof(u16));

		if(tileWidth > TILE_MAX_WIDTH || tileHeight > TILE_MAX_HEIGHT)
			goto cleanup;

		u32 section_size;
		tonccpy(&section_size, ptr + 4, sizeof(u32));
		ptr += 8 + section_size;
	} else {
		goto cleanup;
	}

	// Character data
	if(memcmp(ptr, "CDAT", 4) == 0) {
		fontTiles = new u8[tileHeight * tileCount];
		if(!fontTiles)
			goto cleanup;

		tonccpy(fontTiles, ptr + 8, tileHeight * tileCount);

		u32 section_size;
		tonccpy(&section_size, ptr + 4, sizeof(u32));
		ptr += 8 + section_size;
	} else {
		goto cleanup;
	}

	// character map
	if(memcmp(ptr, "CMAP", 4) == 0) {
		fontMap = new u16[tileCount];
		if(!fontMap)
			goto cleanup;

		tonccpy(fontMap, ptr + 8, sizeof(u16) * tileCount);

		u32 section_size;
		tonccpy(&section_size, ptr + 4, sizeof(u32));
		ptr += 8 + section_size;
	} else {
		goto cleanup;
	}

	questionMark = getCharIndex('?');

	// Copy palette to VRAM
	for(uint i = 0; i < sizeof(palette) / sizeof(palette[0]); i++) {
		tonccpy(BG_PALETTE + i * 0x10, palette[i], 4);
		tonccpy(BG_PALETTE_SUB + i * 0x10, palette[i], 4);
	}

cleanup:
	if(fileBuffer != font_default_frf)
		delete[] fileBuffer;
}

Font::~Font(void) {
	if(fontTiles)
		delete[] fontTiles;

	if(fontMap)
		delete[] fontMap;
}

u16 Font::getCharIndex(char16_t c) {
	// Try a binary search
	int left = 0;
	int right = tileCount;

	while(left <= right) {
		int mid = left + ((right - left) / 2);
		if(fontMap[mid] == c) {
			return mid;
		}

		if(fontMap[mid] < c) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}

	return questionMark;
}

std::u16string Font::utf8to16(std::string_view text) {
	std::u16string out;
	for(uint i = 0; i < text.size();) {
		char16_t c;
		if(!(text[i] & 0x80)) {
			c = text[i++];
		} else if((text[i] & 0xE0) == 0xC0) {
			c  = (text[i++] & 0x1F) << 6;
			c |=  text[i++] & 0x3F;
		} else if((text[i] & 0xF0) == 0xE0) {
			c  = (text[i++] & 0x0F) << 12;
			c |= (text[i++] & 0x3F) << 6;
			c |=  text[i++] & 0x3F;
		} else {
			i++; // out of range or something (This only does up to U+FFFF since it goes to a U16 anyways)
		}
		out += c;
	}
	return out;
}

int Font::calcHeight(std::u16string_view text, int xPos) {
	int lines = 1, chars = xPos + 1;
	for(char16_t c : text) {
		if(c == '\n' || chars > 256 / tileWidth) {
			nocashMessage(c == '\n' ? "line" : "cha");
			lines++;
			chars = xPos + 1;
		} else {
			chars++;
		}
	}
	return lines;
}

void Font::printf(int xPos, int yPos, bool top, Alignment align, Palette palette, const char *format, ...) {
	char str[0x100];
	va_list va;
	va_start(va, format);
	vsniprintf(str, 0x100, format, va);
	va_end(va);

	print(xPos, yPos, top, str, align, palette);
}

ITCM_CODE void Font::print(int xPos, int yPos, bool top, std::u16string_view text, Alignment align, Palette palette, bool rtl) {
	int x = xPos * tileWidth, y = yPos * tileHeight;
	if(x < 0 && align != Alignment::center)
		x += 256;
	if(y < 0)
		y += 192;

	// If RTL isn't forced, check for RTL text
	if(!rtl) {
		for(const auto c : text) {
			if(isStrongRTL(c)) {
				rtl = true;
				break;
			}
		}
	}
	auto ltrBegin = text.end(), ltrEnd = text.end();

	// Adjust x for alignment
	switch(align) {
		case Alignment::left: {
			break;
		} case Alignment::center: {
			size_t newline = text.find('\n');
			while(newline != text.npos) {
				print(xPos, yPos, top, text.substr(0, newline), align, palette, rtl);
				text = text.substr(newline + 1);
				newline = text.find('\n');
				yPos++;
				y += tileHeight;
			}

			x = ((256 - (text.length() * tileWidth)) / 2) + x;
			break;
		} case Alignment::right: {
			size_t newline = text.find('\n');
			while(newline != text.npos) {
				print(xPos, yPos, top, text.substr(0, newline), Alignment::left, palette, rtl);
				text = text.substr(newline + 1);
				newline = text.find('\n');
				yPos++;
				y += tileHeight;
			}
			break;
		}
	}

	if(align == Alignment::right)
		x -= (text.length() - 1) * tileWidth;

	// Align to grid
	x -= x % tileWidth;
	y -= y % tileHeight;
	x += (256 % tileWidth) / 2;
	y += (192 % tileHeight) / 2;

	const int xStart = x;

	// Loop through string and print it
	for(auto it = (rtl ? text.end() - 1 : text.begin()); true; it += (rtl ? -1 : 1)) {
		// If we hit the end of the string in an LTR section of an RTL
		// string, it may not be done, if so jump back to printing RTL
		if(it == (rtl ? text.begin() - 1 : text.end())) {
			if(ltrBegin == text.end() || (ltrBegin == text.begin() && ltrEnd == text.end())) {
				break;
			} else {
				it = ltrBegin;
				ltrBegin = text.end();
				rtl = true;
			}
		}

		// If at the end of an LTR section within RTL, jump back to the RTL
		if(it == ltrEnd && ltrBegin != text.end()) {
			if(ltrBegin == text.begin() && (!isWeak(*ltrBegin) || isNumber(*ltrBegin)))
				break;

			it = ltrBegin;
			ltrBegin = text.end();
			rtl = true;
		// If in RTL and hit a non-RTL character that's not punctuation, switch to LTR
		} else if(rtl && !isStrongRTL(*it) && (!isWeak(*it) || isNumber(*it))) {
			// Save where we are as the end of the LTR section
			ltrEnd = it + 1;

			// Go back until an RTL character or the start of the string
			bool allNumbers = true;
			while(!isStrongRTL(*it) && it != text.begin()) {
				// Check for if the LTR section is only numbers,
				// if so they won't be removed from the end
				if(allNumbers && !isNumber(*it) && !isWeak(*it))
					allNumbers = false;
				it--;
			}

			// Save where we are to return to after printing the LTR section
			ltrBegin = it;

			// If on an RTL char right now, add one
			if(isStrongRTL(*it)) {
				it++;
			}

			// Remove all punctuation and, if the section isn't only numbers,
			// numbers from the end of the LTR section
			if(allNumbers) {
				while(isWeak(*it) && !isNumber(*it)) {
					if(it != text.begin())
						ltrBegin++;
					it++;
				}
			} else {
				while(isWeak(*it)) {
					if(it != text.begin())
						ltrBegin++;
					it++;
				}
			}

			// But then allow all numbers directly touching the strong LTR or with 1 weak between
			while((it - 1 >= text.begin() && isNumber(*(it - 1))) || (it - 2 >= text.begin() && isWeak(*(it - 1)) && isNumber(*(it - 2)))) {
				if(it - 1 != text.begin())
					ltrBegin--;
				it--;
			}

			rtl = false;
		}

		if(*it == '\n') {
			x = xStart;
			y += tileHeight;
			continue;
		}

		// Brackets are flipped in RTL
		u16 index;
		if(rtl) {
			switch(*it) {
				case '(':
					index = getCharIndex(')');
					break;
				case ')':
					index = getCharIndex('(');
					break;
				case '[':
					index = getCharIndex(']');
					break;
				case ']':
					index = getCharIndex('[');
					break;
				case '<':
					index = getCharIndex('>');
					break;
				case '>':
					index = getCharIndex('<');
					break;
				default:
					index = getCharIndex(*it);
					break;
			}
		} else {
			index = getCharIndex(*it);
		}

		// Wrap at right edge if not center aligning
		if(x + tileWidth > 256 && align != Alignment::center) {
			x = xStart;
			y += tileHeight;
		}

		// Don't draw off screen chars
		if(x >= 0 && y >= 0 && y + tileHeight <= 192) {
			u8 *dst = textBuf[top] + x;
			for(int i = 0; i < tileHeight; i++) {
				u8 px = fontTiles[(index * tileHeight) + i];
				for(int j = 0; j < tileWidth; j++) {
					dst[(y + i) * 256 + j] = u8(palette) * 0x10 + ((px >> (7 - j)) & 1);
				}
			}
		}

		x += tileWidth;
	}
}
