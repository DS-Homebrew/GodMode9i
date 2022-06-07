#include "startMenu.h"

#include "config.h"
#include "driveOperations.h"
#include "font.h"
#include "language.h"
#include "main.h"
#include "screenshot.h"
#include "titleManager.h"

#include <array>
#include <nds.h>
#include <vector>

#define ITEMS_PER_SCREEN 8

enum class StartMenuItem : u8 {
	powerOff = 0,
	reboot = 1,
	titleManager = 2,
	langauge = 3,
};

constexpr std::array<std::string *, 4> startMenuStrings = {
	&STR_POWER_OFF,
	&STR_REBOOT,
	&STR_OPEN_TITLE_MANAGER,
	&STR_LANGUAGE
};

constexpr std::array<std::pair<const char *, const char *>, 15> languageList = {{
	{"de-DE", "Deutsch"},
	{"en-US", "English"},
	{"es-ES", "Español"},
	{"fr-FR", "Français"},
	{"it-IT", "Italiano"},
	{"hu-HU", "Magyar"},
	{"nl-NL", "Nederlands"},
	{"ro-RO", "Română"},
	{"tr-TR", "Türkçe"},
	{"ru-RU", "Русский"},
	{"uk-UA", "Українська"},
	{"he-IL", "עברית"},
	{"zh-CN", "中文 (简体)"},
	{"ja-JP", "日本語"},
	{"ja-KANA", "にほんご"}
}};

void startMenu() {
	std::vector<StartMenuItem> startMenuItems;
	if(!isRegularDS) {
		startMenuItems = {
			StartMenuItem::powerOff,
			StartMenuItem::reboot
		};
		if(nandMounted && (sdMounted || flashcardMounted))
			startMenuItems.push_back(StartMenuItem::titleManager);
		startMenuItems.push_back(StartMenuItem::langauge);
	} else {
		startMenuItems = {
			StartMenuItem::powerOff,
			StartMenuItem::langauge
		};
	}

	int cursorPosition = 0;
	u16 pressed, held;
	while(1) {
		font->clear(false);
		font->print(0, 3, false, STR_START_MENU, Alignment::center);
		for(int i = 0; i < (int)startMenuItems.size(); i++) {
			if(cursorPosition == i)
				font->printf(0, 5 + i, false, Alignment::center, Palette::white, "> %s <", startMenuStrings[u8(startMenuItems[i])]->c_str());
			else
				font->print(0, 5 + i, false, startMenuStrings[u8(startMenuItems[i])]->c_str(), Alignment::center);
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
			switch(startMenuItems[cursorPosition]) {
				case StartMenuItem::powerOff:
					systemShutDown();
					break;
				case StartMenuItem::reboot:
					fifoSendValue32(FIFO_USER_02, 1);
					while(1) swiWaitForVBlank();
					break;
				case StartMenuItem::titleManager:
					titleManager();
					break;
				case StartMenuItem::langauge:
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
	if(ownNitroFSMounted != 0) {
		font->clear(false);
		font->print(firstCol, 0, false, ownNitroFSMounted == 1 ? STR_NITROFS_NOT_MOUNTED : STR_NITROFS_UNMOUNTED, alignStart);
		font->print(firstCol, font->calcHeight(ownNitroFSMounted == 1 ? STR_NITROFS_NOT_MOUNTED : STR_NITROFS_UNMOUNTED) + 1, false, STR_A_CONTINUE, alignStart);
		font->update(false);

		do {
			scanKeys();
			swiWaitForVBlank();
		} while (!(keysDownRepeat() & KEY_A));

		return;
	}

	int cursorPosition = 0, scrollPosition = 0;

	for(int i = 0; i < (int)languageList.size(); i++) {
		char iniPath[40];
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
			char iniPath[40];
			snprintf(iniPath, sizeof(iniPath), "nitro:/languages/%s/language.ini", languageList[cursorPosition].first);
			config->languageIniPath(iniPath);
			config->save();
			langInit(true);

			// Reload font if using default
			if(access(config->fontPath().c_str(), F_OK) != 0) {
				if(config->languageIniPath().substr(17, 2) == "zh")
					font = new Font("nitro:/fonts/misaki-gothic-8x8.frf");
				else
					font = new Font(nullptr);
			}
			return;
		} else if(pressed & KEY_B) {
			return;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}
