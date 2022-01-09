#include "titleManager.h"
#include "driveOperations.h"
#include "file_browse.h"
#include "fileOperations.h"
#include "font.h"
#include "language.h"
#include "screenshot.h"

#include <algorithm>
#include <dirent.h>
#include <nds.h>
#include <unistd.h>
#include <vector>

struct TitleInfo {
	TitleInfo(std::string appPath, std::string pubPath, std::string prvPath, const char *gameTitle, const char *gameCode, u8 romVersion, std::u16string bannerTitle) : appPath(appPath), pubPath(pubPath), prvPath(prvPath), romVersion(romVersion), bannerTitle(bannerTitle) {
		strcpy(this->gameTitle, gameTitle);
		strcpy(this->gameCode, gameCode);
	}

	std::string appPath;
	std::string pubPath;
	std::string prvPath;
	char gameTitle[13];
	char gameCode[5];
	u8 romVersion;
	std::u16string bannerTitle;
};

enum TitleDumpOption {
	none = 0,
	rom = 1,
	publicSave = 4,
	privateSave = 8,
	all = rom | publicSave | privateSave
};

void dumpTitle(TitleInfo &title) {
	u16 pressed = 0, held = 0;
	int optionOffset = 0;

	std::vector<TitleDumpOption> allowedOptions({TitleDumpOption::all, TitleDumpOption::rom});
	if(title.pubPath.length() > 0)
		allowedOptions.push_back(TitleDumpOption::publicSave);
	if(title.prvPath.length() > 0)
		allowedOptions.push_back(TitleDumpOption::privateSave);

	char dumpName[32];
	snprintf(dumpName, sizeof(dumpName), "%s_%s_%02X", title.gameTitle, title.gameCode, title.romVersion);

	char dumpToStr[256];
	snprintf(dumpToStr, sizeof(dumpToStr), STR_DUMP_TO.c_str(), dumpName, sdMounted ? "sd" : "fat");

	int y = font->calcHeight(dumpToStr) + 1;

	while (true) {
		font->clear(false);

		font->print(0, 0, false, dumpToStr);

		int row = y;
		for(TitleDumpOption option : allowedOptions) {
			switch(option) {
				case TitleDumpOption::all:
					font->print(3, row++, false, STR_DUMP_ALL);
					break;
				case TitleDumpOption::rom:
					font->print(3, row++, false, STR_DUMP_ROM);
					break;
				case TitleDumpOption::publicSave:
					font->print(3, row++, false, STR_DUMP_PUBLIC_SAVE);
					break;
				case TitleDumpOption::privateSave:
					font->print(3, row++, false, STR_DUMP_PRIVATE_SAVE);
					break;
				case TitleDumpOption::none:
					row++;
					break;
			}
		}

		font->print(3, ++row, false, STR_A_SELECT_B_CANCEL);

		// Show cursor
		font->print(0, y + optionOffset, false, "->");

		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();
		} while (!(pressed & (KEY_UP| KEY_DOWN | KEY_A | KEY_B | KEY_L))
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

		if (pressed & KEY_UP)
			optionOffset--;
		if (pressed & KEY_DOWN)
			optionOffset++;

		if (optionOffset < 0) // Wrap around to bottom of list
			optionOffset = allowedOptions.size() - 1;

		if (optionOffset >= (int)allowedOptions.size()) // Wrap around to top of list
			optionOffset = 0;

		if (pressed & KEY_A) {
			TitleDumpOption selectedOption = allowedOptions[optionOffset];

			// Ensure directories exist
			char folderPath[16];
			sprintf(folderPath, "%s:/gm9i", (sdMounted ? "sd" : "fat"));
			if (access(folderPath, F_OK) != 0) {
				font->clear(false);
				font->print(0, 0, false, STR_CREATING_DIRECTORY);
				font->update(false);
				mkdir(folderPath, 0777);
			}
			sprintf(folderPath, "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
			if (access(folderPath, F_OK) != 0) {
				font->clear(false);
				font->print(0, 0, false, STR_CREATING_DIRECTORY);
				font->update(false);
				mkdir(folderPath, 0777);
			}

			// Dump to /gm9i/out
			char path[64];
			if(selectedOption & TitleDumpOption::rom) {
				snprintf(path, sizeof(path), "%s:/gm9i/out/%s.nds", sdMounted ? "sd" : "fat", dumpName);
				fcopy(title.appPath.c_str(), path);
			}

			if((selectedOption & TitleDumpOption::publicSave) && title.pubPath.length() > 0) {
				snprintf(path, sizeof(path), "%s:/gm9i/out/%s.pub", sdMounted ? "sd" : "fat", dumpName);
				fcopy(title.pubPath.c_str(), path);
			}

			if((selectedOption & TitleDumpOption::privateSave) && title.prvPath.length() > 0) {
				snprintf(path, sizeof(path), "%s:/gm9i/out/%s.prv", sdMounted ? "sd" : "fat", dumpName);
				fcopy(title.prvPath.c_str(), path);
			}

			return;
		}

		if (pressed & KEY_B)
			return;

#ifdef SCREENSWAP
		// Swap screens
		if (pressed & KEY_TOUCH) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}
#endif

		// Make a screenshot
		if ((held & KEY_R) && (pressed & KEY_L)) {
			screenshot();
		}
	}
}

void titleManager() {
	if(!nandMounted || !(sdMounted || flashcardMounted))
		return;

	char oldPath[PATH_MAX];
	getcwd(oldPath, PATH_MAX);

	std::vector<TitleInfo> titles;
	for(u32 tidHigh : {0x00030004, 0x00030005, 0x00030015, 0x00030017}) {
		char path[64];
		snprintf(path, sizeof(path), "nand:/title/%08lx", tidHigh);
		if(access(path, F_OK) == 0) {
			chdir(path);
			std::vector<DirEntry> dirContents;
			getDirectoryContents(dirContents);
			for(const DirEntry &entry : dirContents) {
				if(entry.name[0] == '.')
					continue;

				u8 version;
				snprintf(path, sizeof(path), "nand:/title/%08lx/%s/content/title.tmd", tidHigh, entry.name.c_str());
				FILE *tmd = fopen(path, "rb");
				if(tmd) {
					fseek(tmd, 0x1E7, SEEK_SET);
					fread(&version, sizeof(version), 1, tmd);
					fclose(tmd);

					char gameTitle[13] = {0};
					char gameCode[7] = {0};
					u8 romVersion;
					char16_t title[0x80];
					char pubPath[64], prvPath[64];
					snprintf(path, sizeof(path), "nand:/title/%08lx/%s/content/000000%02x.app", tidHigh, entry.name.c_str(), version);
					FILE *app = fopen(path, "rb");
					if(app) {
						fread(gameTitle, 1, 12, app);
						fread(gameCode, 1, 6, app);
						fseek(app, 12, SEEK_CUR);
						fread(&romVersion, 1, 1, app);

						u32 ofs;
						fseek(app, 0x68, SEEK_SET);
						fread(&ofs, sizeof(u32), 1, app);
						if(ofs >= 0x8000 && fseek(app, ofs, SEEK_SET) == 0) {
							fseek(app, 0x240 + (0x80 * 2), SEEK_CUR);
							fread(title, 2, 0x80, app);
						} else {
							title[0] = u'\0';
						}

						fclose(app);

						// Check if saves exist
						snprintf(pubPath, sizeof(pubPath), "nand:/title/%08lx/%s/data/public.sav", tidHigh, entry.name.c_str());
						if(access(pubPath, F_OK) != 0)
							pubPath[0] = '\0';
						snprintf(prvPath, sizeof(prvPath), "nand:/title/%08lx/%s/data/private.sav", tidHigh, entry.name.c_str());
						if(access(prvPath, F_OK) != 0)
							prvPath[0] = '\0';

						titles.emplace_back(path, pubPath, prvPath, gameTitle, gameCode, romVersion, title);

					}
				}
			}
		}
	}

	chdir(oldPath);

	// Sort alphabetically by banner title
	std::sort(titles.begin(), titles.end(), [](TitleInfo lhs, TitleInfo rhs) {
		for(size_t i = 0; i < lhs.bannerTitle.length(); i++) {
			char16_t lchar = tolower(lhs.bannerTitle[i]);
			char16_t rchar = tolower(rhs.bannerTitle[i]);
			if(lchar == u'\0')
				return true;
			else if(rchar == u'\0')
				return false;
			else if(lchar < rchar)
				return true;
			else if(lchar > rchar)
				return false;
		}

		return false;
	});

	u16 pressed = 0, held = 0;
	int cursorPosition = 0, scrollOffset = 0;
	while(1) {
		font->clear(false);
		font->printf(0, 0, false, Alignment::left, Palette::blackGreen, "%*c", SCREEN_COLS, ' ');
		font->print(0, 0, false, STR_TITLE_MANAGER, Alignment::center, Palette::blackGreen);

		for(int i = 0; i < ((int)titles.size() - scrollOffset) && i < ENTRIES_PER_SCREEN; i++) {
			const TitleInfo &title = titles[scrollOffset + i];
			Palette pal = scrollOffset + i == cursorPosition ? Palette::white : Palette::gray;
			font->print(0, 1 + i, false, title.bannerTitle.substr(0, title.bannerTitle.find(u'\n')), Alignment::left, pal);
			font->printf(-1, 1 + i, false, Alignment::right, pal, " (%s)", title.gameCode);
		}

		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!(held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_L
#ifdef SCREENSWAP
				| KEY_TOUCH
#endif
				)));

		if(held & KEY_UP) {
			cursorPosition--;
			if(cursorPosition < 0)
				cursorPosition = titles.size() - 1;
		} else if(held & KEY_DOWN) {
			cursorPosition++;
			if(cursorPosition > (int)titles.size() - 1)
				cursorPosition = 0;
		} else if(held & KEY_LEFT) {
			cursorPosition -= ENTRIES_PER_SCREEN;
			if(cursorPosition < 0)
				cursorPosition = 0;
		} else if(held & KEY_RIGHT) {
			cursorPosition += ENTRIES_PER_SCREEN;
			if(cursorPosition > (int)titles.size() + 1)
				cursorPosition = titles.size() - 1;
		} else if(pressed & KEY_A) {
			dumpTitle(titles[cursorPosition]);
		} else if(pressed & KEY_B) {
			return;
		}

		// Scroll screen if needed
		if (cursorPosition < scrollOffset)
			scrollOffset = cursorPosition;
		if (cursorPosition > scrollOffset + ENTRIES_PER_SCREEN - 1)
			scrollOffset = cursorPosition - ENTRIES_PER_SCREEN + 1;

#ifdef SCREENSWAP
		// Swap screens
		if (pressed & KEY_TOUCH) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}
#endif

		if((pressed & KEY_L) && (keysHeld() & KEY_R)) {
			screenshot();
		}
	}
}