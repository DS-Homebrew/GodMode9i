#include "startMenu.h"

#include "config.h"
#include "font.h"
#include "language.h"
#include "screenshot.h"

#include <array>
#include <nds.h>

#define ITEMS_PER_SCREEN 8

constexpr std::array<std::string *, 3> startMenuItems = {
	&STR_POWER_OFF,
	&STR_REBOOT,
	&STR_LANGUAGE
};

constexpr std::array<std::pair<const char *, const char *>, 3> languageList = {{
	{"en-US", "English"},
	{"es-ES", "Español"},
	{"ja-JP", "日本語"}
}};

void startMenu() {
	int cursorPosition = 0;
	u16 pressed, held;
	while(1) {
		font->clear(false);
		font->print(0, 3, false, STR_START_MENU, Alignment::center);
		for(int i = 0; i < (int)startMenuItems.size(); i++) {
			if(cursorPosition == i)
				font->printf(0, 5 + i, false, Alignment::center, Palette::white, "> %s <", startMenuItems[i]->c_str());
			else
				font->print(0, 5 + i, false, startMenuItems[i]->c_str(), Alignment::center);
		}
		font->print(0, 5 + startMenuItems.size() + 1, false, STR_A_SELECT_B_CANCEL, Alignment::center);
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!held);

		if(held & KEY_UP) {
			cursorPosition--;
			if(cursorPosition < 0)
				cursorPosition = startMenuItems.size() - 1;
		} else if(held & KEY_DOWN) {
			cursorPosition++;
			if(cursorPosition >= (int)startMenuItems.size())
				cursorPosition = 0;
		} else if(pressed & KEY_A) {
			switch(cursorPosition) {
				case 0: // Power off
					// TODO
					break;
				case 1: // Reboot
					fifoSendValue32(FIFO_USER_02, 1);
					while(1) swiWaitForVBlank();
					break;
				case 2: // language
					languageMenu();
					break;
			}
		} else if(pressed & KEY_B) {
			return;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}

void languageMenu() {
	int cursorPosition = 0, scrollPosition = 0;

	for(int i = 0; i < (int)languageList.size(); i++) {
		char iniPath[36];
		snprintf(iniPath, sizeof(iniPath), "nitro:/languages/%s/language.ini", languageList[i].first);
		if(config->languageIniPath() == iniPath) {
			cursorPosition = i;
			break;
		}
	}

	u16 pressed, held;
	while(1) {
		if(cursorPosition - scrollPosition >= ITEMS_PER_SCREEN) {
			scrollPosition = cursorPosition - ITEMS_PER_SCREEN + 1;
		} else if(cursorPosition < scrollPosition) {
			scrollPosition = cursorPosition;
		}

		font->clear(false);
		font->print(0, 3, false, STR_SELECT_LANGUAGE, Alignment::center);
		for(int i = 0; i < ITEMS_PER_SCREEN && i < (int)languageList.size(); i++) {
			if(cursorPosition == scrollPosition + i)
				font->printf(0, 5 + i, false, Alignment::center, Palette::white, "> %s <", languageList[scrollPosition + i].second);
			else
				font->print(0, 5 + i, false, languageList[scrollPosition + i].second, Alignment::center);
		}
		font->print(0, 5 + std::min(ITEMS_PER_SCREEN, (int)languageList.size()) + 1, false, STR_A_SELECT_B_CANCEL, Alignment::center);
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!held);

		if(held & KEY_UP) {
			cursorPosition--;
			if(cursorPosition < 0)
				cursorPosition = languageList.size() - 1;
		} else if(held & KEY_DOWN) {
			cursorPosition++;
			if(cursorPosition >= (int)languageList.size())
				cursorPosition = 0;
		} else if(pressed & KEY_A) {
			char iniPath[36];
			snprintf(iniPath, sizeof(iniPath), "nitro:/languages/%s/language.ini", languageList[cursorPosition].first);
			config->languageIniPath(iniPath);
			config->save();
			langInit(true);
			return;
		} else if(pressed & KEY_B) {
			return;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}
