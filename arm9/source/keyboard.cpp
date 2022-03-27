#include "keyboard.h"
#include "font.h"
#include "language.h"
#include "main.h"

#include <nds.h>
#include <string.h>

std::string kbdGetString(std::string label, int maxSize, std::string oldStr) {
#ifdef SCREENSWAP
	lcdMainOnTop();
#endif

	font->clear(false);
	font->update(false);

	bgInit(0, BgType_Text4bpp, BgSize_T_256x512, 20, 0);
	keyboardInit(nullptr, 0, BgType_Text4bpp, BgSize_T_256x512, 20, 0, false, true);
	BG_PALETTE_SUB[0] = 0x0000;
	BG_PALETTE_SUB[1] = 0x7FFF;
	keyboardShow();

	std::string output(oldStr);

	u16 pressed;
	int key, cursorPosition = output.length();
	int labelHeight = font->calcHeight(label);
	bool done = false;
	while(!done) {
		font->clear(false);
		font->print(firstCol, 0, false, label, alignStart);
		font->printf(0, labelHeight, false, Alignment::left, Palette::white, "> %s", output.c_str());
		font->printf(2 + cursorPosition, labelHeight, false, Alignment::left, Palette::blackWhite, "%c", cursorPosition < (int)output.length() ? output[cursorPosition] : ' ');
		font->print(firstCol, labelHeight + 2, false, STR_START_RETURN_B_BACKSPACE, alignStart);
		font->update(false);

		do {
			scanKeys();
			pressed = keysDownRepeat();
			key = keyboardUpdate();
			swiWaitForVBlank();
		} while (!((pressed & (KEY_LEFT | KEY_RIGHT | KEY_B | KEY_START)) || (key != -1)));

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
				if(cursorPosition < (int)output.length())
					cursorPosition++;
				break;
			case DVK_LEFT: // Left
				if(cursorPosition > 0)
					cursorPosition--;
				break;
			case DVK_FOLD: // (using as esc)
				output = oldStr;
				done = true;
				break;
			case DVK_BACKSPACE: // Backspace
				if(cursorPosition > 0) {
					output.erase(output.begin() + cursorPosition - 1);
					cursorPosition--;
				}
				break;
			case DVK_ENTER: // Return
				done = true;
				break;
			default: // Letter
				if(output.size() < (uint)maxSize) {
					output.insert(output.begin() + cursorPosition, key);
					cursorPosition++;
				}
				break;
		}

		if(pressed & KEY_LEFT) {
			if(cursorPosition > 0)
				cursorPosition--;
		} else if(pressed & KEY_RIGHT) {
			if(cursorPosition < (int)output.length())
				cursorPosition++;
		} else if(pressed & KEY_B) {
			if(cursorPosition > 0) {
				output.erase(output.begin() + cursorPosition - 1);
				cursorPosition--;
			}
		} else if(pressed & KEY_START) {
			done = true;
		}
	}
	keyboardHide();

#ifdef SCREENSWAP
	screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
#endif

	return output;
}
