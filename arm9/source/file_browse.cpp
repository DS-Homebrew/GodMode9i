/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.

------------------------------------------------------------------*/

#include "file_browse.h"
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>

#include "main.h"
#include "config.h"
#include "date.h"
#include "screenshot.h"
#include "fileOperations.h"
#include "driveMenu.h"
#include "driveOperations.h"
#include "dumpOperations.h"
#include "font.h"
#include "hexEditor.h"
#include "my_sd.h"
#include "keyboard.h"
#include "ndsInfo.h"
#include "startMenu.h"
#include "nitrofs.h"
#include "inifile.h"
#include "nds_loader_arm9.h"
#include "language.h"

#define ENTRIES_START_ROW 1
#define OPTIONS_ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

static char path[PATH_MAX];

bool extension(const std::string_view filename, const std::vector<std::string_view> &extensions) {
	for(const std::string_view &ext : extensions) {
		if(filename.length() >= ext.length() && strcasecmp(filename.substr(filename.length() - ext.length()).data(), ext.data()) == 0)
			return true;
	}

	return false;
}

bool dirEntryPredicate (const DirEntry& lhs, const DirEntry& rhs) {

	if (!lhs.isDirectory && rhs.isDirectory) {
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory) {
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

bool getDirectoryContents(std::vector<DirEntry>& dirContents) {
	dirContents.clear();

	DIR *pdir = opendir (".");

	if (pdir == nullptr) {
		font->print(firstCol, 0, true, STR_UNABLE_TO_OPEN_DIRECTORY, alignStart);
		font->update(true);
		return false;
	} else {
		while (true) {
			dirent *pent = readdir(pdir);
			if (pent == nullptr)
				break;

			if (strcmp(pent->d_name, ".") == 0 || strcmp(pent->d_name, "..") == 0)
				continue;

			bool isApp = false;
			if (extension(pent->d_name, {"nds", "argv", "dsi", "ids", "app", "srl"})) {
				isApp = (currentDrive == Drive::sdCard && sdMounted) || (currentDrive == Drive::flashcard && flashcardMounted);
			} else if (extension(pent->d_name, {"firm"})) {
				isApp = (is3DS && sdMounted);
			}

			dirContents.emplace_back(pent->d_name, pent->d_type == DT_DIR ? 0 : -1, pent->d_type == DT_DIR, isApp);
		}
		closedir(pdir);
	}

	std::sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);

	// Add ".." to top of list
	dirContents.insert(dirContents.begin(), {"..", 0, true, false});

	return true;
}

void showDirectoryContents(std::vector<DirEntry> &dirContents, int fileOffset, int startRow) {
	getcwd(path, PATH_MAX);

	font->clear(true);

	// Top bar
	font->printf(firstCol, 0, true, alignStart, Palette::blackGreen, "%*c", 256 / font->width(), ' ');

	std::string time = RetTime();

	// Print the path
	if(font->calcWidth(path) > SCREEN_COLS - 6)
		font->print(rtl ? -1 : (-1 - time.size()), 0, true, path, Alignment::right, Palette::blackGreen, true);
	else
		font->print(firstCol, 0, true, path, alignStart, Palette::blackGreen);

	// Print time
	font->print(lastCol, 0, true, time, alignEnd, Palette::blackGreen);

	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		DirEntry *entry = &dirContents[i + startRow];

		Palette pal;
		if ((fileOffset - startRow) == i) {
			pal = Palette::white;
		} else if (entry->selected) {
			pal = Palette::yellow;
		} else if (entry->isDirectory) {
			pal = Palette::blue;
		} else {
			pal = Palette::gray;
		}

		// Load size if not loaded yet
		if(entry->size == -1)
			entry->size = getFileSize(entry->name.c_str());

		int nameSize = 0;
		for(int i = 0; i < SCREEN_COLS; nameSize++) {
			if((entry->name[nameSize] & 0xC0) != 0x80)
				i++;
		}

		font->print(firstCol, i + 1, true, entry->name.substr(0, nameSize), alignStart, pal);
		if (entry->name == "..") {
			font->print(lastCol, i + 1, true, "(..)", alignEnd, pal);
		} else if (entry->isDirectory) {
			font->print(lastCol, i + 1, true, " " + STR_DIR, alignEnd, pal);
		} else {
			font->printf(lastCol, i + 1, true, alignEnd, pal, " (%s)", getBytes(entry->size).c_str());
		}
	}

	font->update(true);
}

FileOperation fileBrowse_A(DirEntry* entry, char path[PATH_MAX]) {
	if(config->screenSwap())
		lcdMainOnTop();

	int pressed = 0, held = 0;
	std::vector<FileOperation> operations;
	int optionOffset = 0;
	std::string fullPath = path + entry->name;
	int y = font->calcHeight(fullPath) + 1;

	if (!entry->isDirectory) {
		if (entry->isApp) {
			operations.push_back(FileOperation::bootFile);
		}

		if(extension(entry->name, {"nds", "dsi", "ids", "app", "srl"})) {
			if(currentDrive != Drive::nitroFS)
				operations.push_back(FileOperation::mountNitroFS);
			operations.push_back(FileOperation::ndsInfo);
			operations.push_back(FileOperation::trimNds);
		}
		if(extension(entry->name, {"sav", "sav1", "sav2", "sav3", "sav4", "sav5", "sav6", "sav7", "sav8", "sav9"})) {
			if(!(io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS) || entry->size <= (1 << 20))
				operations.push_back(FileOperation::restoreSaveNds);
			if(isRegularDS && (entry->size == 512 || entry->size == 8192 || entry->size == 32768 || entry->size == 65536 || entry->size == 131072 
				|| entry->size == 528 || entry->size == 8208 || entry->size == 32784 || entry->size == 65552 || entry->size == 131088))
				operations.push_back(FileOperation::restoreSaveGba);
		}
		if(currentDrive != Drive::fatImg && extension(entry->name, {"img", "sd", "sav", "pub", "pu1", "pu2", "pu3", "pu4", "pu5", "pu6", "pu7", "pu8", "pu9", "prv", "pr1", "pr2", "pr3", "pr4", "pr5", "pr6", "pr7", "pr8", "pr9", "0000"})) {
			operations.push_back(FileOperation::mountImg);
		}
		if(extension(entry->name, {"frf"})) {
			operations.push_back(FileOperation::loadFont);
		}

		operations.push_back(FileOperation::hexEdit);

		// The bios SHA1 functions are only available on the DSi
		// https://problemkaputt.de/gbatek.htm#biossha1functionsdsionly
		if (bios9iEnabled) {
			operations.push_back(FileOperation::calculateSHA1);
		}
	}

	operations.push_back(FileOperation::showInfo);

	if (sdMounted && (strcmp(path, "sd:/gm9i/out/") != 0)) {
		operations.push_back(FileOperation::copySdOut);
	}

	if (flashcardMounted && (strcmp(path, "fat:/gm9i/out/") != 0)) {
		operations.push_back(FileOperation::copyFatOut);
	}

	while (true) {
		font->clear(false);

		font->print(firstCol, 0, false, fullPath, alignStart);

		int optionsCol = rtl ? -4 : 3;
		int row = y;
		for(FileOperation operation : operations) {
			switch(operation) {
				case FileOperation::bootFile:
					font->print(optionsCol, row++, false, extension(entry->name, {"firm"}) ? STR_BOOT_FILE : STR_BOOT_FILE_DIRECT, alignStart);
					break;
				case FileOperation::mountNitroFS:
					font->print(optionsCol, row++, false, STR_MOUNT_NITROFS, alignStart);
					break;
				case FileOperation::ndsInfo:
					font->print(optionsCol, row++, false, STR_SHOW_NDS_INFO, alignStart);
					break;
				case FileOperation::trimNds:
					font->print(optionsCol, row++, false, STR_TRIM_NDS, alignStart);
					break;
				case FileOperation::restoreSaveNds:
					if(!isRegularDS)
						font->print(optionsCol, row++, false, STR_RESTORE_SAVE, alignStart);
					else
						font->print(optionsCol, row++, false, STR_RESTORE_SAVE_NDS, alignStart);
					break;
				case FileOperation::restoreSaveGba:
					font->print(optionsCol, row++, false, STR_RESTORE_SAVE_GBA, alignStart);
					break;
				case FileOperation::mountImg:
					font->print(optionsCol, row++, false, STR_MOUNT_FAT_IMG, alignStart);
					break;
				case FileOperation::hexEdit:
					font->print(optionsCol, row++, false, STR_OPEN_HEX, alignStart);
					break;
				case FileOperation::showInfo:
					font->print(optionsCol, row++, false, entry->isDirectory ? STR_SHOW_DIRECTORY_INFO : STR_SHOW_FILE_INFO, alignStart);
					break;
				case FileOperation::copySdOut:
					font->print(optionsCol, row++, false, STR_COPY_SD_OUT, alignStart);
					break;
				case FileOperation::copyFatOut:
					font->print(optionsCol, row++, false, STR_COPY_FAT_OUT, alignStart);
					break;
				case FileOperation::calculateSHA1:
					font->print(optionsCol, row++, false, STR_CALC_SHA1, alignStart);
					break;
				case FileOperation::loadFont:
					font->print(optionsCol, row++, false, STR_LOAD_FONT, alignStart);
					break;
				case FileOperation::none:
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

			if(driveRemoved(currentDrive)) {
				if(config->screenSwap())
					screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

				return FileOperation::none;
			}
		} while (!(pressed & (KEY_UP| KEY_DOWN | KEY_A | KEY_B | KEY_L)));

		if (pressed & KEY_UP)		optionOffset -= 1;
		if (pressed & KEY_DOWN)		optionOffset += 1;

		if (optionOffset < 0) // Wrap around to bottom of list
			optionOffset = operations.size() - 1;

		if (optionOffset >= (int)operations.size()) // Wrap around to top of list
			optionOffset = 0;

		if (pressed & KEY_A) {
			switch(operations[optionOffset]) {
				case FileOperation::bootFile: {
					applaunch = true;
					font->print(optionsCol, optionOffset + y, false, STR_LOADING, alignStart);
					font->update(false);
					break;
				} case FileOperation::restoreSaveNds: {
					ndsCardSaveRestore(entry->name.c_str());
					break;
				} case FileOperation::restoreSaveGba: {
					gbaCartSaveRestore(entry->name.c_str());
					break;
				} case FileOperation::copySdOut: {
					if (access("sd:/gm9i", F_OK) != 0) {
						font->print(optionsCol, optionOffset + y, false, STR_CREATING_DIRECTORY, alignStart);
						font->update(false);
						mkdir("sd:/gm9i", 0777);
					}
					if (access("sd:/gm9i/out", F_OK) != 0) {
						font->print(optionsCol, optionOffset + y, false, STR_CREATING_DIRECTORY, alignStart);
						font->update(false);
						mkdir("sd:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "sd:/gm9i/out/%s", entry->name.c_str());
					font->print(optionsCol, optionOffset + y, false, STR_COPYING, alignStart);
					font->update(false);
					remove(destPath);
					char sourceFolder[PATH_MAX];
					getcwd(sourceFolder, PATH_MAX);
					char sourcePath[PATH_MAX];
					snprintf(sourcePath, sizeof(sourcePath), "%s%s", sourceFolder, entry->name.c_str());
					fcopy(sourcePath, destPath);
					chdir(sourceFolder);	// For after copying a folder
					break;
				} case FileOperation::copyFatOut: {
					if (access("fat:/gm9i", F_OK) != 0) {
						font->print(optionsCol, optionOffset + y, false, STR_CREATING_DIRECTORY, alignStart);
						font->update(false);
						mkdir("fat:/gm9i", 0777);
					}
					if (access("fat:/gm9i/out", F_OK) != 0) {
						font->print(optionsCol, optionOffset + y, false, STR_CREATING_DIRECTORY, alignStart);
						font->update(false);
						mkdir("fat:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s", entry->name.c_str());
					font->print(optionsCol, (optionOffset + y), false, STR_COPYING, alignStart);
					font->update(false);
					remove(destPath);
					char sourceFolder[PATH_MAX];
					getcwd(sourceFolder, PATH_MAX);
					char sourcePath[PATH_MAX];
					snprintf(sourcePath, sizeof(sourcePath), "%s%s", sourceFolder, entry->name.c_str());
					fcopy(sourcePath, destPath);
					chdir(sourceFolder);	// For after copying a folder
					break;
				} case FileOperation::mountNitroFS: {
					if(nitroMounted)
						nitroUnmount();

					ownNitroFSMounted = 2;
					nitroMounted = nitroFSInit(entry->name.c_str());
					if (nitroMounted) {
						chdir("nitro:/");
						nitroCurrentDrive = currentDrive;
						currentDrive = Drive::nitroFS;
					}
					break;
				} case FileOperation::ndsInfo: {
					ndsInfo(entry->name.c_str());
					break;
				} case FileOperation::trimNds: {
					entry->size = trimNds(entry->name.c_str());
					break;
				} case FileOperation::showInfo: {
					changeFileAttribs(entry);
					break;
				} case FileOperation::mountImg: {
					if(imgMounted)
						imgUnmount();

					imgMounted = imgMount(entry->name.c_str(), !extension(entry->name, {"img", "sd"}));
					if (imgMounted) {
						chdir("img:/");
						imgCurrentDrive = currentDrive;
						currentDrive = Drive::fatImg;
					}
					break;
				} case FileOperation::hexEdit: {
					hexEditor(entry->name.c_str(), currentDrive);
					break;
				} case FileOperation::loadFont: {
					delete font;
					font = new Font(entry->name.c_str());

					// Reload language to update button characters
					langInit(true);
					break;
				} case FileOperation::calculateSHA1: {
					u8 sha1[20] = {0};
					bool ret = calculateSHA1(strcat(getcwd(path, PATH_MAX), entry->name.c_str()), sha1);
					if (!ret)
						break;

					font->clear(false);
					font->print(firstCol, 0, false, STR_SHA1_HASH_IS, alignStart);
					char sha1Str[41];
					for (int i = 0; i < 20; ++i)
						sniprintf(sha1Str + i * 2, 3, "%02X", sha1[i]);
					font->print(firstCol, 1, false, sha1Str, alignStart);
					font->print(firstCol, font->calcHeight(sha1Str) + 2, false, STR_A_CONTINUE, alignStart);
					font->update(false);

					// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
					int pressed;
					do {
						scanKeys();
						pressed = keysDownRepeat();
						swiWaitForVBlank();

						if(keysHeld() & KEY_R && pressed & KEY_L) {
							screenshot();
						}
					} while (!(pressed & (KEY_A | KEY_Y | KEY_B | KEY_X)));
					break;
				} case FileOperation::none: {
					break;
				}
			}
			keysDownRepeat(); // prevent unwanted key repeat

			if(config->screenSwap())
				screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

			return operations[optionOffset];
		} else if (pressed & KEY_B) {
			if(config->screenSwap())
				screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

			return FileOperation::none;
		}
		// Make a screenshot
		else if ((held & KEY_R) && (pressed & KEY_L)) {
			screenshot();
		}
	}
}

bool fileBrowse_paste(char dest[256]) {
	if(config->screenSwap())
		lcdMainOnTop();

	int pressed = 0;
	int optionOffset = 0;

	while (true) {
		font->clear(false);

		font->print(firstCol, 0, false, STR_PASTE_CLIPBOARD_HERE, alignStart);

		int optionsCol = rtl ? -4 : 3;
		int row = OPTIONS_ENTRIES_START_ROW, maxCursors = 0;
		font->print(optionsCol, row++, false, STR_COPY_FILES, alignStart);
		for (auto &file : clipboard) {
			if (!driveWritable(file.drive))
				continue;
			maxCursors++;
			font->print(optionsCol, row++, false, STR_MOVE_FILES, alignStart);
			break;
		}
		font->print(optionsCol, ++row, false, STR_A_SELECT_B_CANCEL, alignStart);

		// Show cursor
		font->print(firstCol, optionOffset + OPTIONS_ENTRIES_START_ROW, false, rtl ? "<-" : "->", alignStart);

		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!(pressed & (KEY_UP | KEY_DOWN | KEY_A | KEY_B)));

		if (pressed & KEY_UP)		optionOffset -= 1;
		if (pressed & KEY_DOWN)		optionOffset += 1;

		if (optionOffset < 0)				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			font->print(optionsCol, optionOffset + OPTIONS_ENTRIES_START_ROW, false, optionOffset ? STR_MOVING : STR_COPYING, alignStart);
			for (auto &file : clipboard) {
				std::string destPath = dest + file.name;
				if (file.path == destPath)
					continue;	// If the source and destination for the clipped file is the same skip it

				if (optionOffset && driveWritable(file.drive)) {	 // Don't remove if from read-only drive
					if (currentDrive == file.drive) {
						rename(file.path.c_str(), destPath.c_str());
					} else {
						fcopy(file.path.c_str(), destPath.c_str());		// Copy file to destination, since renaming won't work
						remove(file.path.c_str());				// Delete source file after copying
					}
				} else {
					remove(destPath.c_str());
					fcopy(file.path.c_str(), destPath.c_str());
				}
			}
			clipboardUsed = true;		// Disable clipboard restore
			clipboardOn = false;	// Clear clipboard after copying or moving

			if(config->screenSwap())
				screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

			return true;
		}
		if (pressed & KEY_B) {
			if(config->screenSwap())
				screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();

			return false;
		}
	}
}

void recRemove(const char *path, std::vector<DirEntry> dirContents) {
	if(chdir(path) == 0 && getDirectoryContents(dirContents)) {
		for (int i = 1; i < ((int)dirContents.size()); i++) {
			DirEntry &entry = dirContents[i];
			if (entry.isDirectory)
				recRemove(entry.name.c_str(), dirContents);
			if (!(FAT_getAttr(entry.name.c_str()) & ATTR_READONLY)) {
				remove(entry.name.c_str());
			}
		}
		chdir("..");
		remove(path);
	}
}

void fileBrowse_drawBottomScreen(DirEntry* entry) {
	font->clear(false);

	int row = -1;

	if (!isDSiMode() && isRegularDS) {
		font->print(firstCol, row--, false, STR_POWERTEXT_DS, alignStart);
	} else if (is3DS) {
		font->print(firstCol, row--, false, STR_HOMETEXT, alignStart);
		font->print(firstCol, row--, false, STR_POWERTEXT_3DS, alignStart);
	} else {
		font->print(firstCol, row--, false, STR_POWERTEXT, alignStart);
	}
	font->print(firstCol, row--, false, STR_START_START_MENU, alignStart);
	font->print(firstCol, row--, false, clipboardOn ? STR_CLEAR_CLIPBOARD : STR_RESTORE_CLIPBOARD, alignStart);
	if ((sdMounted && driveWritable(Drive::sdCard)) || (flashcardMounted && driveWritable(Drive::flashcard))) {
		font->print(firstCol, row--, false, STR_SCREENSHOTTEXT, alignStart);
	}
	font->print(firstCol, row--, false, STR_DIRECTORY_OPTIONS, alignStart);
	if(driveWritable(currentDrive))
		font->print(firstCol, row--, false, clipboardOn ? STR_PASTE_FILES_CREATE_ENTRY : STR_COPY_FILES_CREATE_ENTRY, alignStart);
	else if(!clipboardOn)
		font->print(firstCol, row--, false, STR_COPY_FILE, alignStart);
	font->print(firstCol, row--, false, entry->selected ? STR_DESELECT_FILES : STR_SELECT_FILES, alignStart);
	if(driveWritable(currentDrive))
		font->print(firstCol, row--, false, STR_DELETE_RENAME_FILE, alignStart);
	font->print(firstCol, row--, false, titleName, alignStart);

	// Load size if not loaded yet
	if(entry->size == -1)
		entry->size = getFileSize(entry->name.c_str());

	Palette pal = entry->selected ? Palette::yellow : (entry->isDirectory ? Palette::blue : Palette::gray);
	font->print(firstCol, 0, false, entry->name, alignStart, pal);
	if (entry->name != "..") {
		if (entry->isDirectory) {
			font->print(firstCol, font->calcHeight(entry->name), false, STR_DIR, alignStart, pal);
		} else if (entry->size == 1) {
			font->print(firstCol, font->calcHeight(entry->name), false, STR_1_BYTE, alignStart, pal);
		} else {
			font->printf(firstCol, font->calcHeight(entry->name), false, alignStart, pal, STR_N_BYTES.c_str(), entry->size);
		}
	}
	if (clipboardOn) {
		font->print(firstCol, 4, false, STR_CLIPBOARD, alignStart);
		for (size_t i = 0; i < clipboard.size(); ++i) {
			if (i < 4) {
				font->print(firstCol, 5 + i, false, clipboard[i].name, alignStart, clipboard[i].folder ? Palette::blue : Palette::gray);
			} else {
				font->printf(firstCol, 5 + i, false, alignStart, Palette::gray, clipboard.size() - 4 == 1 ? STR_1_MORE_FILE.c_str() : STR_N_MORE_FILES.c_str(), clipboard.size() - 4);
				break;
			}
		}
	}

	font->update(false);
}

std::string browseForFile (void) {
	int pressed = 0;
	int held = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	std::vector<DirEntry> dirContents;

	getDirectoryContents (dirContents);

	while (true) {
		DirEntry* entry = &dirContents[fileOffset];

		fileBrowse_drawBottomScreen(entry);
		showDirectoryContents(dirContents, fileOffset, screenOffset);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();

			if(driveRemoved(currentDrive)) {
				screenMode = 0;
				return "null";
			}
		} while (!(pressed & ~(KEY_R | KEY_LID)));

		if (pressed & KEY_UP) {
			fileOffset--;
			if(fileOffset < 0)
				fileOffset = dirContents.size() - 1;
		} else if (pressed & KEY_DOWN) {
			fileOffset++;
			if(fileOffset > (int)dirContents.size() - 1)
				fileOffset = 0;
		} else if (pressed & KEY_LEFT) {
			fileOffset -= ENTRY_PAGE_LENGTH;
			if(fileOffset < 0)
				fileOffset = 0;
		} else if (pressed & KEY_RIGHT) {
			fileOffset += ENTRY_PAGE_LENGTH;
			if(fileOffset > (int)dirContents.size() - 1)
				fileOffset = dirContents.size() - 1;
		}


		// Scroll screen if needed
		if (fileOffset < screenOffset)	{
			screenOffset = fileOffset;
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
		}

		getcwd(path, PATH_MAX);

		if ((!(held & KEY_R) && (pressed & KEY_A))
		|| (!entry->isDirectory && (held & KEY_R) && (pressed & KEY_A))) {
			if (entry->name == ".." && strcmp(path, getDrivePath()) == 0) {
				screenMode = 0;
				return "null";
			} else if (entry->isDirectory) {
				font->printf(firstCol, fileOffset - screenOffset + ENTRIES_START_ROW, true, alignStart, Palette::white, "%-*s", SCREEN_COLS - 5, STR_ENTERING_DIRECTORY.c_str(), alignStart);
				font->update(true);
				// Enter selected directory
				chdir(entry->name.c_str());
				getDirectoryContents(dirContents);
				screenOffset = 0;
				fileOffset = 0;
			} else {
				FileOperation getOp = fileBrowse_A(entry, path);
				if(getOp == FileOperation::bootFile) {
					// Return the chosen file
					return entry->name;
				} else if (getOp == FileOperation::copySdOut
						|| getOp == FileOperation::copyFatOut
						|| (getOp == FileOperation::mountNitroFS && nitroMounted)
						|| (getOp == FileOperation::mountImg && imgMounted)) {
					getDirectoryContents(dirContents); // Refresh directory listing
					if ((getOp == FileOperation::mountNitroFS && nitroMounted)
					 || (getOp == FileOperation::mountImg && imgMounted)) {
						screenOffset = 0;
						fileOffset = 0;
					}
				}
			}
		} else if (entry->isDirectory && (held & KEY_R) && (pressed & KEY_A)) { // Directory options
			if (entry->name == "..") {
				screenMode = 0;
				return "null";
			} else {
				FileOperation getOp = fileBrowse_A(entry, path);
				if (getOp == FileOperation::copySdOut || getOp == FileOperation::copyFatOut) {
					getDirectoryContents (dirContents);		// Refresh directory listing
				} else if (getOp == FileOperation::showInfo) {
					for (int i = 0; i < 15; i++) swiWaitForVBlank();
				}
			}
		} else if (pressed & KEY_B) {
			if (strcmp(path, getDrivePath()) == 0) {
				screenMode = 0;
				return "null";
			}
			// Go up a directory
			chdir("..");
			getDirectoryContents(dirContents);
			screenOffset = 0;
			fileOffset = 0;

			// Return selection to where it was
			char *trailingSlash = strrchr(path, '/');
			*trailingSlash = '\0';
			std::string dirName = strrchr(path, '/') + 1;
			*trailingSlash = '/';
			for(size_t i = 0; i < dirContents.size(); i++) {
				if(dirContents[i].name == dirName) {
					fileOffset = i;
					if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1)
						screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
					break;
				}
			}
		} else if ((held & KEY_R) && (pressed & KEY_X) && (entry->name != ".." && driveWritable(currentDrive))) { // Rename file/folder
			pressed = 0;

			std::string newName = kbdGetString(STR_RENAME_TO, -1, entry->name);

			if (newName.length() > 0) {
				// Check for unsupported characters
				for (uint i = 0; i < newName.length(); i++) {
					switch(newName[i]) {
						case '>':
						case '<':
						case ':':
						case '"':
						case '/':
						case '\\':
						case '|':
						case '?':
						case '*':
						newName[i] = '_'; // Remove unsupported character
					}
				}
				if (rename(entry->name.c_str(), newName.c_str()) == 0) {
					getDirectoryContents(dirContents);
				}
			}
		} else if ((pressed & KEY_X) && (entry->name != ".." && driveWritable(currentDrive))) { // Delete action
			font->clear(false);
			int selections = std::count_if(dirContents.begin(), dirContents.end(), [](const DirEntry &x){ return x.selected; });
			if (entry->selected && selections > 1) {
				font->printf(firstCol, 0, false, alignStart, Palette::white, STR_DELETE_N_PATHS.c_str(), selections);
				for (uint i = 0, printed = 0; i < dirContents.size() && printed < 5; i++) {
					if (dirContents[i].selected) {
						font->printf(firstCol, printed + 2, false, alignStart, Palette::red, "- %s", dirContents[i].name.c_str());
						printed++;
					}
				}
				if(selections > 5)
					font->printf(firstCol, 7, false, alignStart, Palette::red, selections - 5 == 1 ? STR_AND_1_MORE.c_str() : STR_AND_N_MORE.c_str(), selections - 5);
			} else {
				font->printf(firstCol, 0, false, alignStart, Palette::white, STR_DELETE_X.c_str(), entry->name.c_str());
			}
			font->print(firstCol, (!entry->selected || selections == 1) ? 2 : (selections > 5 ? 9 : selections + 3), false, STR_A_YES_B_NO, alignStart);
			font->update(false);

			while (true) {
				scanKeys();
				pressed = keysDownRepeat();
				swiWaitForVBlank();
				if (pressed & KEY_A) {
					if (entry->selected) {
						font->clear(false);
						font->print(firstCol, 0, false, STR_DELETING_FILES, alignStart);
						font->update(false);
						struct stat st;
						for (auto &item : dirContents) {
							if(item.selected) {
								if (FAT_getAttr(item.name.c_str()) & ATTR_READONLY)
									continue;
								stat(item.name.c_str(), &st);
								if (st.st_mode & S_IFDIR)
									recRemove(item.name.c_str(), dirContents);
								else
									remove(item.name.c_str());
							}
						}
						fileOffset = 0;
					} else if (FAT_getAttr(entry->name.c_str()) & ATTR_READONLY) {
						font->clear(false);
						font->printf(firstCol, 0, false, alignStart, Palette::white, STR_FAILED_DELETING.c_str(), entry->name.c_str());
						font->print(firstCol, 3, false, STR_A_CONTINUE, alignStart);
						pressed = 0;

						while (!(pressed & KEY_A)) {
							scanKeys();
							pressed = keysDown();
							swiWaitForVBlank();
						}
						for (int i = 0; i < 15; i++) swiWaitForVBlank();
					} else {
						if (entry->isDirectory) {
							font->clear(false);
							font->print(firstCol, 0, false, STR_DELETING_FOLDER, alignStart);
							font->update(false);
							recRemove(entry->name.c_str(), dirContents);
						} else {
							font->clear(false);
							font->print(firstCol, 0, false, STR_DELETING_FILES, alignStart);
							font->update(false);
							remove(entry->name.c_str());
						}
						fileOffset--;
					}
					getDirectoryContents (dirContents);
					pressed = 0;
					break;
				}
				if (pressed & KEY_B) {
					pressed = 0;
					break;
				}
			}
		} else if ((held & KEY_R) && (pressed & KEY_Y) && driveWritable(currentDrive)) { // Create new folder
			pressed = 0;

			std::string newName = kbdGetString(STR_NAME_FOR_NEW_FOLDER);

			if (newName.length() > 0) {
				// Check for unsupported characters
				for (uint i = 0; i < newName.length(); i++) {
					switch(newName[i]) {
						case '>':
						case '<':
						case ':':
						case '"':
						case '/':
						case '\\':
						case '|':
						case '?':
						case '*':
						newName[i] = '_'; // Remove unsupported character
					}
				}
				if (mkdir(newName.c_str(), 0777) == 0) {
					getDirectoryContents (dirContents);
				}
			}
		} else if ((pressed & KEY_L && !(held & KEY_R)) && entry->name != "..") { // Add to selection
			bool select = !entry->selected;
			entry->selected = select;
			while(held & KEY_L) {
				do {
					scanKeys();
					pressed = keysDownRepeat();
					held = keysHeld();
					swiWaitForVBlank();
				} while ((held & KEY_L) && !(pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)));

				if(pressed & (KEY_UP | KEY_DOWN)) {
					if (pressed & KEY_UP) {
						fileOffset--;
						if(fileOffset < 0) {
							fileOffset = dirContents.size() - 1;
						} else {
							entry = &dirContents[fileOffset];
							if(entry->name != "..")
								entry->selected = select;
						}
					} else if (pressed & KEY_DOWN) {
						fileOffset++;
						if(fileOffset > (int)dirContents.size() - 1) {
							fileOffset = 0;
						} else {
							entry = &dirContents[fileOffset];
							if(entry->name != "..")
								entry->selected = select;
						}
					}

					// Scroll screen if needed
					if (fileOffset < screenOffset)	{
						screenOffset = fileOffset;
					} else if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
						screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
					}
				}
				
				if(pressed & KEY_LEFT) {
					for(auto &item : dirContents) {
						if(item.name != "..")
							item.selected = false;
					}
				} else if(pressed & KEY_RIGHT) {
					for(auto &item : dirContents) {
						if(item.name != "..")
							item.selected = true;
					}
				}

				fileBrowse_drawBottomScreen(entry);
				showDirectoryContents(dirContents, fileOffset, screenOffset);
			}
		} else if (pressed & KEY_Y) {
			// Copy
			if (!clipboardOn) {
				if (entry->name != "..") {
					clipboardOn = true;
					clipboardUsed = false;
					clipboard.clear();
					if (entry->selected) {
						for (auto &item : dirContents) {
							if(item.selected) {
								clipboard.emplace_back(path + item.name, item.name, item.isDirectory, currentDrive);
								item.selected = false;
							}
						}
					} else {
						clipboard.emplace_back(path + entry->name, entry->name, entry->isDirectory, currentDrive);
					}
				}
			// Paste
			} else if (driveWritable(currentDrive) && fileBrowse_paste(path)) {
				getDirectoryContents (dirContents);
			}
		} else if ((pressed & KEY_SELECT) && !clipboardUsed) {
			clipboardOn = !clipboardOn;
		} if (pressed & KEY_START) { // START menu
			startMenu();
		} else if (pressed & config->screenSwapKey()) { // Swap screens
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		} else if ((held & KEY_R) && (pressed & KEY_L)) { // Make a screenshot
			if(screenshot())
				getDirectoryContents(dirContents);
		}
	}
}
