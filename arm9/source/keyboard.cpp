#include "keyboard.h"
#include "config.h"
#include "font.h"
#include "language.h"
#include "main.h"

#include <nds.h>
#include <string.h>

std::string kbdGetString(std::string label, int maxSize, std::string oldStr) {
	if(config->screenSwap())
		lcdMainOnTop();

	font->clear(false);
	font->update(false);

	bgInit(0, BgType_Text4bpp, BgSize_T_256x512, 20, 0);
	keyboardInit(nullptr, 0, BgType_Text4bpp, BgSize_T_256x512, 20, 0, false, true);
	BG_PALETTE_SUB[0] = 0x0000;
	BG_PALETTE_SUB[1] = 0x7FFF;
	keyboardShow();

	std::string output(oldStr);

	int stringPosition = output.size(), scrollPosition = stringPosition;
	for(int i = 0; i < SCREEN_COLS - 4 && scrollPosition > 0; i++) {
		scrollPosition--;
		while((output[scrollPosition] & 0xC0) == 0x80) // UTF-8
			scrollPosition--;
	}

	u16 pressed;
	int key;
	int labelHeight = font->calcHeight(label);
	bool done = false;
	while(!done) {
		font->clear(false);
		font->print(firstCol, 0, false, label, alignStart);

		int strSize = 0;
		for(int i = 0; strSize < (int)output.size() && (i < SCREEN_COLS - 3 || (output[scrollPosition + strSize] & 0xC0) == 0x80); strSize++) {
			if((output[scrollPosition + strSize] & 0xC0) != 0x80)
				i++;
		}
		font->printf(0, labelHeight, false, Alignment::left, Palette::white, "> %s", output.substr(scrollPosition, strSize).c_str());

		if(scrollPosition + strSize < (int)output.size())
			font->print(-1, labelHeight, false, "→");
		if(scrollPosition > 0)
			font->print(1, labelHeight, false, "←");

		int charLen = 1;
		while((output[stringPosition + charLen] & 0xC0) == 0x80)
			charLen++;
		int cursorPosition = 0;
		for(int i = 0; scrollPosition + i < stringPosition; i++) {
			if((output[scrollPosition + i] & 0xC0) != 0x80)
				cursorPosition++;
		}
		font->printf(2 + cursorPosition, labelHeight, false, Alignment::left, Palette::blackWhite, "%s", stringPosition < (int)output.size() ? output.substr(stringPosition, charLen).c_str() : " ");

		font->print(firstCol, labelHeight + 2, false, STR_START_RETURN_B_BACKSPACE, alignStart);
		font->update(false);

		do {
			scanKeys();
			pressed = keysDownRepeat();
			key = keyboardUpdate();
			swiWaitForVBlank();
		} while (!((pressed & (KEY_LEFT | KEY_RIGHT | KEY_B | KEY_START | KEY_TOUCH)) || (key != -1)));

		switch(key) {
			case NOKEY:
			case DVK_MENU:
			case DVK_CAPS: // Caps
			case DVK_SHIFT: // Shift
			case DVK_CTRL: // Ctrl
			case DVK_UP: // Up
			case DVK_DOWN: // Down
			case DVK_ALT: // Alt
			case DVK_TAB: // tab
				break;
			case DVK_RIGHT: // Right
				pressed |= KEY_RIGHT;
				break;
			case DVK_LEFT: // Left
				pressed |= KEY_LEFT;
				break;
			case DVK_FOLD: // (using as esc)
				output = oldStr;
				done = true;
				break;
			case DVK_BACKSPACE: // Backspace
				pressed |= KEY_B;
				break;
			case DVK_ENTER: // Return
				done = true;
				break;
			default: // Letter
				if(output.size() < (uint)maxSize) {
					output.insert(output.begin() + stringPosition, key);
					stringPosition++;

					if(cursorPosition + 1 >= (SCREEN_COLS - 3) && stringPosition <= (int)output.size()) {
						scrollPosition++;
						while((output[scrollPosition] & 0xC0) == 0x80) // UTF-8
							scrollPosition++;
					}
				}
				break;
		}

		if(pressed & KEY_TOUCH) {
			touchPosition touch;
			touchRead(&touch);
			int px = touch.px - (256 % font->width()) / 2;
			int py = touch.py - (192 % font->height()) / 2;
			if(py >= font->height() && py < font->height() * 2) {
				if(px < font->width() * 2) {
					pressed |= KEY_LEFT;
				} else if(px > 256 - font->width()) {
					pressed |= KEY_RIGHT;
				} else {
					int pos = (px / font->width()) - 2;
					while(stringPosition - scrollPosition > pos && stringPosition > 0) {
						stringPosition--;
						while((output[stringPosition] & 0xC0) == 0x80) // UTF-8
							stringPosition--;
					}
					while(stringPosition - scrollPosition < pos && stringPosition < (int)output.size()) {
						stringPosition++;
						while((output[stringPosition] & 0xC0) == 0x80) // UTF-8
							stringPosition++;
					}
				}

			}
		}

		if(pressed & KEY_LEFT) {
			if(stringPosition > 0) {
				stringPosition--;
				while((output[stringPosition] & 0xC0) == 0x80) // UTF-8
					stringPosition--;

				if(cursorPosition - 1 < 0) {
					scrollPosition--;
					while((output[scrollPosition] & 0xC0) == 0x80) // UTF-8
						scrollPosition--;
				}
			}
		} else if(pressed & KEY_RIGHT) {
			if(stringPosition < (int)output.size()) {
				stringPosition++;
				while((output[stringPosition] & 0xC0) == 0x80) // UTF-8
					stringPosition++;

				if(cursorPosition + 1 >= (SCREEN_COLS - 3)) {
					scrollPosition++;
					while((output[scrollPosition] & 0xC0) == 0x80) // UTF-8
						scrollPosition++;
				}
			}
		} else if(pressed & KEY_B) {
			if(stringPosition > 0) {
				stringPosition--;
				while((output[stringPosition] & 0xC0) == 0x80) {
					output.erase(output.begin() + stringPosition);
					stringPosition--;
				}
				output.erase(output.begin() + stringPosition);

				if(cursorPosition - 1 < 0) {
					scrollPosition--;
					while((output[scrollPosition] & 0xC0) == 0x80) // UTF-8
						scrollPosition--;
				}
			}
		} else if(pressed & KEY_START) {
			done = true;
		}
	}
	keyboardHide();

	if(config->screenSwap())
		screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

	return output;
}
