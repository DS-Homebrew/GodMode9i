#include "titleManager.h"
#include "config.h"
#include "driveOperations.h"
#include "file_browse.h"
#include "fileOperations.h"
#include "font.h"
#include "language.h"
#include "main.h"
#include "screenshot.h"

#include <algorithm>
#include <dirent.h>
#include <nds.h>
#include <unistd.h>
#include <vector>

struct TitleInfo {
	TitleInfo(std::string path, const char *gameTitle, const char *gameCode, u8 *appVersion, u8 romVersion, std::u16string bannerTitle) : path(path), romVersion(romVersion), bannerTitle(bannerTitle) {
		strcpy(this->gameTitle, gameTitle);
		strcpy(this->gameCode, gameCode);
		tonccpy(this->appVersion, appVersion, 4);
	}

	std::string path;
	char gameTitle[13];
	char gameCode[7];
	u8 appVersion[4];
	u8 romVersion;
	std::u16string bannerTitle;
};

enum TitleDumpOption {
	none = 0,
	rom = 1,
	publicSave = 4,
	privateSave = 8,
	bannerSave = 16,
	tmd = 32,
	all = rom | publicSave | privateSave | bannerSave | tmd
};

void dumpTitle(TitleInfo &title) {
	u16 pressed = 0, held = 0;
	int optionOffset = 0;

	std::vector<TitleDumpOption> allowedOptions({TitleDumpOption::all, TitleDumpOption::rom});
	u8 allowedBitfield = TitleDumpOption::rom | TitleDumpOption::tmd;
	if(access((title.path + "/data/public.sav").c_str(), F_OK) == 0) {
		allowedOptions.push_back(TitleDumpOption::publicSave);
		allowedBitfield |= TitleDumpOption::publicSave;
	}
	if(access((title.path + "/data/private.sav").c_str(), F_OK) == 0) {
		allowedOptions.push_back(TitleDumpOption::privateSave);
		allowedBitfield |= TitleDumpOption::privateSave;
	}
	if(access((title.path + "/data/banner.sav").c_str(), F_OK) == 0) {
		allowedOptions.push_back(TitleDumpOption::bannerSave);
		allowedBitfield |= TitleDumpOption::bannerSave;
	}
	allowedOptions.push_back(TitleDumpOption::tmd);

	char dumpName[32];
	snprintf(dumpName, sizeof(dumpName), "%s_%s_%02X", title.gameTitle, title.gameCode, title.romVersion);

	char dumpToStr[256];
	snprintf(dumpToStr, sizeof(dumpToStr), STR_DUMP_TO.c_str(), dumpName, getDefaultDrivePath());

	int y = font->calcHeight(dumpToStr) + 1;

	while (true) {
		font->clear(false);

		font->print(firstCol, 0, false, dumpToStr, alignStart);

		int optionsCol = rtl ? -4 : 3;
		int row = y;
		for(TitleDumpOption option : allowedOptions) {
			switch(option) {
				case TitleDumpOption::all:
					font->print(optionsCol, row++, false, STR_DUMP_ALL, alignStart);
					break;
				case TitleDumpOption::rom:
					font->print(optionsCol, row++, false, STR_DUMP_ROM, alignStart);
					break;
				case TitleDumpOption::publicSave:
					font->print(optionsCol, row++, false, STR_DUMP_PUBLIC_SAVE, alignStart);
					break;
				case TitleDumpOption::privateSave:
					font->print(optionsCol, row++, false, STR_DUMP_PRIVATE_SAVE, alignStart);
					break;
				case TitleDumpOption::bannerSave:
					font->print(optionsCol, row++, false, STR_DUMP_BANNER_SAVE, alignStart);
					break;
				case TitleDumpOption::tmd:
					font->print(optionsCol, row++, false, STR_DUMP_TMD, alignStart);
					break;
				case TitleDumpOption::none:
					row++;
					break;
			}
		}

		font->print(optionsCol, ++row, false, STR_A_SELECT_B_CANCEL, alignStart);

		// Show cursor
		font->print(firstCol, y + optionOffset, false, rtl ? "<-" : "->", alignStart);

		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();
		} while (!(pressed & (KEY_UP| KEY_DOWN | KEY_A | KEY_B | KEY_L | config->screenSwapKey())));

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
			sprintf(folderPath, "%s:/gm9i", getDefaultDrivePath());
			if (access(folderPath, F_OK) != 0) {
				font->clear(false);
				font->print(firstCol, 0, false, STR_CREATING_DIRECTORY, alignStart);
				font->update(false);
				mkdir(folderPath, 0777);
			}
			sprintf(folderPath, "%s:/gm9i/out", getDefaultDrivePath());
			if (access(folderPath, F_OK) != 0) {
				font->clear(false);
				font->print(firstCol, 0, false, STR_CREATING_DIRECTORY, alignStart);
				font->update(false);
				mkdir(folderPath, 0777);
			}

			// Dump to /gm9i/out
			char inpath[64], outpath[64];
			if((selectedOption & TitleDumpOption::rom) && (allowedBitfield & TitleDumpOption::rom)) {
				snprintf(inpath, sizeof(inpath), "%s/content/%02x%02x%02x%02x.app", title.path.c_str(), title.appVersion[0], title.appVersion[1], title.appVersion[2], title.appVersion[3]);
				snprintf(outpath, sizeof(outpath), "%s:/gm9i/out/%s.nds", getDefaultDrivePath(), dumpName);
				fcopy(inpath, outpath);
			}

			if((selectedOption & TitleDumpOption::publicSave) && (allowedBitfield & TitleDumpOption::publicSave)) {
				snprintf(inpath, sizeof(inpath), "%s/data/public.sav", title.path.c_str());
				snprintf(outpath, sizeof(outpath), "%s:/gm9i/out/%s.pub", getDefaultDrivePath(), dumpName);
				fcopy(inpath, outpath);
			}

			if((selectedOption & TitleDumpOption::privateSave) && (allowedBitfield & TitleDumpOption::privateSave)) {
				snprintf(inpath, sizeof(inpath), "%s/data/private.sav", title.path.c_str());
				snprintf(outpath, sizeof(outpath), "%s:/gm9i/out/%s.prv", getDefaultDrivePath(), dumpName);
				fcopy(inpath, outpath);
			}

			if((selectedOption & TitleDumpOption::bannerSave) && (allowedBitfield & TitleDumpOption::bannerSave)) {
				snprintf(inpath, sizeof(inpath), "%s/data/banner.sav", title.path.c_str());
				snprintf(outpath, sizeof(outpath), "%s:/gm9i/out/%s.bnr", getDefaultDrivePath(), dumpName);
				fcopy(inpath, outpath);
			}

			if((selectedOption & TitleDumpOption::tmd) && (allowedBitfield & TitleDumpOption::tmd)) {
				snprintf(inpath, sizeof(inpath), "%s/content/title.tmd", title.path.c_str());
				snprintf(outpath, sizeof(outpath), "%s:/gm9i/out/%s.tmd", getDefaultDrivePath(), dumpName);
				fcopy(inpath, outpath);
			}

			return;
		}

		if (pressed & KEY_B)
			return;

		// Swap screens
		if (pressed & config->screenSwapKey()) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}

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

				u8 appVersion[4];
				snprintf(path, sizeof(path), "nand:/title/%08lx/%s/content/title.tmd", tidHigh, entry.name.c_str());
				FILE *tmd = fopen(path, "rb");
				if(tmd) {
					fseek(tmd, 0x1E4, SEEK_SET);
					fread(appVersion, 1, 4, tmd);
					fclose(tmd);

					snprintf(path, sizeof(path), "nand:/title/%08lx/%s/content/%02x%02x%02x%02x.app", tidHigh, entry.name.c_str(), appVersion[0], appVersion[1], appVersion[2], appVersion[3]);
					FILE *app = fopen(path, "rb");
					if(app) {
						char gameTitle[13] = {0};
						char gameCode[7] = {0};
						u8 romVersion;
						fread(gameTitle, 1, 12, app);
						fread(gameCode, 1, 6, app);
						fseek(app, 12, SEEK_CUR);
						fread(&romVersion, 1, 1, app);

						u32 ofs;
						char16_t title[0x80];
						fseek(app, 0x68, SEEK_SET);
						fread(&ofs, sizeof(u32), 1, app);
						if(ofs >= 0x8000 && fseek(app, ofs, SEEK_SET) == 0) {
							fseek(app, 0x240 + (0x80 * 2), SEEK_CUR);
							fread(title, 2, 0x80, app);
						} else {
							title[0] = u'\0';
						}

						fclose(app);

						snprintf(path, sizeof(path), "nand:/title/%08lx/%s", tidHigh, entry.name.c_str());
						titles.emplace_back(path, gameTitle, gameCode, appVersion, romVersion, title);
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
		font->printf(firstCol, 0, false, alignStart, Palette::blackGreen, "%*c", SCREEN_COLS, ' ');
		font->print(0, 0, false, STR_TITLE_MANAGER, Alignment::center, Palette::blackGreen);

		for(int i = 0; i < ((int)titles.size() - scrollOffset) && i < ENTRIES_PER_SCREEN; i++) {
			const TitleInfo &title = titles[scrollOffset + i];
			Palette pal = scrollOffset + i == cursorPosition ? Palette::white : Palette::gray;
			font->print(firstCol, 1 + i, false, title.bannerTitle.substr(0, title.bannerTitle.find(u'\n')), alignStart, pal);
			font->printf(lastCol, 1 + i, false, alignEnd, pal, rtl ? "(%s) " : " (%s)", title.gameCode);
		}

		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!(held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_L | config->screenSwapKey())));

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

		// Swap screens
		if (pressed & config->screenSwapKey()) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}

		if((pressed & KEY_L) && (keysHeld() & KEY_R)) {
			screenshot();
		}
	}
}