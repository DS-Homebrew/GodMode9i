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
		if(filename.length() > ext.length() && strcasecmp(filename.substr(filename.length() - ext.length()).data(), ext.data()) == 0)
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

void getDirectoryContents(std::vector<DirEntry>& dirContents) {
	dirContents.clear();

	DIR *pdir = opendir (".");

	if (pdir == nullptr) {
		font->print(0, 0, true, STR_UNABLE_TO_OPEN_DIRECTORY);
		font->update(true);
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
				isApp = (isDSiMode() && is3DS && sdMounted);
			}

			dirContents.emplace_back(pent->d_name, pent->d_type == DT_DIR ? 0 : -1, pent->d_type == DT_DIR, isApp);
		}
		closedir(pdir);
	}

	std::sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);

	// Add ".." to top of list
	dirContents.insert(dirContents.begin(), {"..", 0, true, false});
}

void showDirectoryContents(std::vector<DirEntry> &dirContents, int fileOffset, int startRow) {
	getcwd(path, PATH_MAX);

	font->clear(true);

	// Top bar
	font->printf(0, 0, true, Alignment::left, Palette::blackGreen, "%*c", 256 / font->width(), ' ');

	// Print time
	font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);

	// Print the path
	if(font->calcWidth(path) > SCREEN_COLS - 6)
		font->print(-6 - 1, 0, true, path, Alignment::right, Palette::blackGreen);
	else
		font->print(0, 0, true, path, Alignment::left, Palette::blackGreen);

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

		font->print(0, i + 1, true, entry->name.substr(0, nameSize), Alignment::left, pal);
		if (entry->name == "..") {
			font->print(-1, i + 1, true, "(..)", Alignment::right, pal);
		} else if (entry->isDirectory) {
			font->print(-1, i + 1, true, " " + STR_DIR, Alignment::right, pal);
		} else {
			font->printf(-1, i + 1, true, Alignment::right, pal, " (%s)", getBytes(entry->size).c_str());
		}
	}

	font->update(true);
}

FileOperation fileBrowse_A(DirEntry* entry, char path[PATH_MAX]) {
	int pressed = 0, held = 0;
	std::vector<FileOperation> operations;
	int optionOffset = 0;
	std::string fullPath = path + entry->name;
	int y = font->calcHeight(fullPath) + 1;

	if (!entry->isDirectory) {
		if (entry->isApp) {
			operations.push_back(FileOperation::bootFile);
			if (!extension(entry->name, {"firm"})) {
				operations.push_back(FileOperation::bootstrapFile);
			}
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
			if(isRegularDS && (entry->size == 512 || entry->size == 8192 || entry->size == 32768 || entry->size == 65536 || entry->size == 131072))
				operations.push_back(FileOperation::restoreSaveGba);
		}
		if(currentDrive != Drive::fatImg && extension(entry->name, {"img", "sd", "sav", "pub", "pu1", "pu2", "pu3", "pu4", "pu5", "pu6", "pu7", "pu8", "pu9", "prv", "pr1", "pr2", "pr3", "pr4", "pr5", "pr6", "pr7", "pr8", "pr9"})) {
			operations.push_back(FileOperation::mountImg);
		}
		if(extension(entry->name, {"frf"})) {
			operations.push_back(FileOperation::loadFont);
		}

		operations.push_back(FileOperation::hexEdit);

		// The bios SHA1 functions are only available on the DSi
		// https://problemkaputt.de/gbatek.htm#biossha1functionsdsionly
		if (isDSiMode()) {
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

		font->print(0, 0, false, fullPath);

		int row = y;
		for(FileOperation operation : operations) {
			switch(operation) {
				case FileOperation::bootFile:
					font->print(3, row++, false, extension(entry->name, {"firm"}) ? STR_BOOT_FILE : STR_BOOT_FILE_DIRECT);
					break;
				case FileOperation::bootstrapFile:
					font->print(3, row++, false, STR_BOOTSTRAP_FILE);
					break;
				case FileOperation::mountNitroFS:
					font->print(3, row++, false, STR_MOUNT_NITROFS);
					break;
				case FileOperation::ndsInfo:
					font->print(3, row++, false, STR_SHOW_NDS_INFO);
					break;
				case FileOperation::trimNds:
					font->print(3, row++, false, STR_TRIM_NDS);
					break;
				case FileOperation::restoreSaveNds:
					if(!isRegularDS)
						font->print(3, row++, false, STR_RESTORE_SAVE);
					else
						font->print(3, row++, false, STR_RESTORE_SAVE_NDS);
					break;
				case FileOperation::restoreSaveGba:
					font->print(3, row++, false, STR_RESTORE_SAVE_GBA);
					break;
				case FileOperation::mountImg:
					font->print(3, row++, false, STR_MOUNT_FAT_IMG);
					break;
				case FileOperation::hexEdit:
					font->print(3, row++, false, STR_OPEN_HEX);
					break;
				case FileOperation::showInfo:
					font->print(3, row++, false, entry->isDirectory ? STR_SHOW_DIRECTORY_INFO : STR_SHOW_FILE_INFO);
					break;
				case FileOperation::copySdOut:
					font->print(3, row++, false, STR_COPY_SD_OUT);
					break;
				case FileOperation::copyFatOut:
					font->print(3, row++, false, STR_COPY_FAT_OUT);
					break;
				case FileOperation::calculateSHA1:
					font->print(3, row++, false, STR_CALC_SHA1);
					break;
				case FileOperation::loadFont:
					font->print(3, row++, false, STR_LOAD_FONT);
					break;
				case FileOperation::none:
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

			if(driveRemoved(currentDrive))
				return FileOperation::none;
		} while (!(pressed & (KEY_UP| KEY_DOWN | KEY_A | KEY_B | KEY_L))
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

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
					font->print(3, optionOffset + y, false, STR_LOADING);
					font->update(false);
					break;
				} case FileOperation::bootstrapFile: {
					char baseFile[256], savePath[PATH_MAX]; //, bootstrapConfigPath[32];
					//snprintf(bootstrapConfigPath, 32, "%s:/_nds/nds-bootstrap.ini", isDSiMode() ? "sd" : "fat");
					strncpy(baseFile, entry->name.c_str(), 255);
					*strrchr(baseFile, '.') = 0;
					snprintf(savePath, PATH_MAX, "%s%s%s.sav", path, !access("saves", F_OK) ? "saves/" : "", baseFile);
					CIniFile bootstrapConfig("/_nds/nds-bootstrap.ini");
					bootstrapConfig.SetString("NDS-BOOTSTRAP", "NDS_PATH", fullPath);
					bootstrapConfig.SetString("NDS-BOOTSTRAP", "SAV_PATH", savePath);
					bootstrapConfig.SetInt("NDS-BOOTSTRAP", "DSI_MODE", 0);
					bootstrapConfig.SaveIniFile("/_nds/nds-bootstrap.ini");
					// TODO Something less hacky lol
					chdir(isDSiMode()&&sdMounted ? "sd:/_nds" : "fat:/_nds");
					// TODO Read header and check for homebrew flag, based on that runNdsFile nds-bootstrap(-hb)-release
					entry->name = "nds-bootstrap-release.nds";
					applaunch = true;
					return FileOperation::bootFile;
					break;
				} case FileOperation::restoreSaveNds: {
					ndsCardSaveRestore(entry->name.c_str());
					break;
				} case FileOperation::restoreSaveGba: {
					gbaCartSaveRestore(entry->name.c_str());
					break;
				} case FileOperation::copySdOut: {
					if (access("sd:/gm9i", F_OK) != 0) {
						font->print(3, optionOffset + y, false, STR_CREATING_DIRECTORY);
						font->update(false);
						mkdir("sd:/gm9i", 0777);
					}
					if (access("sd:/gm9i/out", F_OK) != 0) {
						font->print(3, optionOffset + y, false, STR_CREATING_DIRECTORY);
						font->update(false);
						mkdir("sd:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "sd:/gm9i/out/%s", entry->name.c_str());
					font->print(3, optionOffset + y, false, STR_COPYING);
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
						font->print(3, optionOffset + y, false, STR_CREATING_DIRECTORY);
						font->update(false);
						mkdir("fat:/gm9i", 0777);
					}
					if (access("fat:/gm9i/out", F_OK) != 0) {
						font->print(3, optionOffset + y, false, STR_CREATING_DIRECTORY);
						font->update(false);
						mkdir("fat:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s", entry->name.c_str());
					font->print(3, (optionOffset + y), false, STR_COPYING);
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
					font->print(0, 0, false, STR_SHA1_HASH_IS);
					char sha1Str[41];
					for (int i = 0; i < 20; ++i)
						sniprintf(sha1Str + i * 2, 3, "%02X", sha1[i]);
					font->print(0, 1, false, sha1Str);
					font->print(0, font->calcHeight(sha1Str) + 2, false, STR_A_CONTINUE);
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
			return operations[optionOffset];
		}
		if (pressed & KEY_B) {
			return FileOperation::none;
		}
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

bool fileBrowse_paste(char dest[256]) {
	int pressed = 0;
	int optionOffset = 0;

	while (true) {
		font->clear(false);

		font->print(0, 0, false, STR_PASTE_CLIPBOARD_HERE);

		int row = OPTIONS_ENTRIES_START_ROW, maxCursors = 0;
		font->print(3, row++, false, STR_COPY_FILES);
		for (auto &file : clipboard) {
			if (!driveWritable(file.drive))
				continue;
			maxCursors++;
			font->print(3, row++, false, STR_MOVE_FILES);
			break;
		}
		font->print(3, ++row, false, STR_A_SELECT_B_CANCEL);

		// Show cursor
		font->print(0, optionOffset + OPTIONS_ENTRIES_START_ROW, false, "->");

		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN)
				&& !(pressed & KEY_A) && !(pressed & KEY_B)
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

		if (pressed & KEY_UP)		optionOffset -= 1;
		if (pressed & KEY_DOWN)		optionOffset += 1;

		if (optionOffset < 0)				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			font->print(3, optionOffset + OPTIONS_ENTRIES_START_ROW, false, optionOffset ? STR_MOVING : STR_COPYING);
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
			return true;
		}
		if (pressed & KEY_B) {
			return false;
		}
#ifdef SCREENSWAP
		// Swap screens
		if (pressed & KEY_TOUCH) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}
#endif
	}
}

void recRemove(const char *path, std::vector<DirEntry> dirContents) {
	chdir (path);
	getDirectoryContents(dirContents);
	for (int i = 1; i < ((int)dirContents.size()); i++) {
		DirEntry &entry = dirContents[i];
		if (entry.isDirectory)
			recRemove(entry.name.c_str(), dirContents);
		if (!(FAT_getAttr(entry.name.c_str()) & ATTR_READONLY)) {
			remove(entry.name.c_str());
		}
	}
	chdir ("..");
	remove(path);
}

void fileBrowse_drawBottomScreen(DirEntry* entry) {
	font->clear(false);

	int row = -1;

	if (!isDSiMode() && isRegularDS) {
		font->print(0, row--, false, STR_POWERTEXT_DS);
	} else if (is3DS) {
		font->print(0, row--, false, STR_HOMETEXT);
		font->print(0, row--, false, STR_POWERTEXT_3DS);
	} else {
		font->print(0, row--, false, STR_POWERTEXT);
	}
	font->print(0, row--, false, STR_START_START_MENU);
	font->print(0, row--, false, clipboardOn ? STR_CLEAR_CLIPBOARD : STR_RESTORE_CLIPBOARD);
	if ((sdMounted && driveWritable(Drive::sdCard)) || (flashcardMounted && driveWritable(Drive::flashcard))) {
		font->print(0, row--, false, STR_SCREENSHOTTEXT);
	}
	font->print(0, row--, false, STR_DIRECTORY_OPTIONS);
	if(driveWritable(currentDrive))
		font->print(0, row--, false, clipboardOn ? STR_PASTE_FILES_CREATE_ENTRY : STR_COPY_FILES_CREATE_ENTRY);
	else if(!clipboardOn)
		font->print(0, row--, false, STR_COPY_FILE);
	font->print(0, row--, false, entry->selected ? STR_DESELECT_FILES : STR_SELECT_FILES);
	if(driveWritable(currentDrive))
		font->print(0, row--, false, STR_DELETE_RENAME_FILE);
	font->print(0, row--, false, titleName);

	// Load size if not loaded yet
	if(entry->size == -1)
		entry->size = getFileSize(entry->name.c_str());

	Palette pal = entry->selected ? Palette::yellow : (entry->isDirectory ? Palette::blue : Palette::gray);
	font->print(0, 0, false, entry->name, Alignment::left, pal);
	if (entry->name != "..") {
		if (entry->isDirectory) {
			font->print(0, font->calcHeight(entry->name), false, STR_DIR, Alignment::left, pal);
		} else if (entry->size == 1) {
			font->print(0, font->calcHeight(entry->name), false, STR_1_BYTE, Alignment::left, pal);
		} else {
			font->printf(0, font->calcHeight(entry->name), false, Alignment::left, pal, STR_N_BYTES.c_str(), entry->size);
		}
	}
	if (clipboardOn) {
		font->print(0, 4, false, STR_CLIPBOARD);
		for (size_t i = 0; i < clipboard.size(); ++i) {
			if (i < 4) {
				font->print(0, 5 + i, false, clipboard[i].name, Alignment::left, clipboard[i].folder ? Palette::blue : Palette::gray);
			} else {
				font->printf(0, 5 + i, false, Alignment::left, Palette::gray, clipboard.size() - 4 == 1 ? STR_1_MORE_FILE.c_str() : STR_N_MORE_FILES.c_str(), clipboard.size() - 4);
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
		} while (!(pressed & ~(KEY_R | KEY_TOUCH | KEY_LID)));

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
			if (entry->name == ".." && strcmp(path, getDrivePath()) == 0)
			{
				screenMode = 0;
				return "null";
			} else if (entry->isDirectory) {
				font->printf(0, fileOffset - screenOffset + ENTRIES_START_ROW, true, Alignment::left, Palette::white, "%-*s", SCREEN_COLS - 5, STR_ENTERING_DIRECTORY.c_str());
				font->update(true);
				// Enter selected directory
				chdir (entry->name.c_str());
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
				} else if(getOp == FileOperation::showInfo) {
					for (int i = 0; i < 15; i++) swiWaitForVBlank();
				}
			}
		}

		// Directory options
		if (entry->isDirectory && (held & KEY_R) && (pressed & KEY_A)) {
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
		}

		if (pressed & KEY_B) {
			if (strcmp(path, getDrivePath()) == 0) {
				screenMode = 0;
				return "null";
			}
			// Go up a directory
			chdir ("..");
			getDirectoryContents (dirContents);
			screenOffset = 0;
			fileOffset = 0;
		}

		// Rename file/folder
		if ((held & KEY_R) && (pressed & KEY_X) && (entry->name != ".." && driveWritable(currentDrive))) {
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
		}

		// Delete action
		if ((pressed & KEY_X) && (entry->name != ".." && driveWritable(currentDrive))) {
			font->clear(false);
			int selections = std::count_if(dirContents.begin(), dirContents.end(), [](const DirEntry &x){ return x.selected; });
			if (entry->selected && selections > 1) {
				font->printf(0, 0, false, Alignment::left, Palette::white, STR_DELETE_N_PATHS.c_str(), selections);
				for (uint i = 0, printed = 0; i < dirContents.size() && printed < 5; i++) {
					if (dirContents[i].selected) {
						font->printf(0, printed + 2, false, Alignment::left, Palette::red, "- %s", dirContents[i].name.c_str());
						printed++;
					}
				}
				if(selections > 5)
					font->printf(0, 7, false, Alignment::left, Palette::red, selections - 5 == 1 ? STR_AND_1_MORE.c_str() : STR_AND_N_MORE.c_str(), selections - 5);
			} else {
				font->printf(0, 0, false, Alignment::left, Palette::white, STR_DELETE_X.c_str(), entry->name.c_str());
			}
			font->print(0, (!entry->selected || selections == 1) ? 2 : (selections > 5 ? 9 : selections + 3), false, STR_A_YES_B_NO);
			font->update(false);

			while (true) {
				scanKeys();
				pressed = keysDownRepeat();
				swiWaitForVBlank();
				if (pressed & KEY_A) {
					if (entry->selected) {
						font->clear(false);
						font->print(0, 0, false, STR_DELETING_FILES);
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
						font->printf(0, 0, false, Alignment::left, Palette::white, STR_FAILED_DELETING.c_str(), entry->name.c_str());
						font->print(0, 3, false, STR_A_CONTINUE);
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
							font->print(0, 0, false, STR_DELETING_FOLDER);
							font->update(false);
							recRemove(entry->name.c_str(), dirContents);
						} else {
							font->clear(false);
							font->print(0, 0, false, STR_DELETING_FILES);
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
		}

		// Create new folder
		if ((held & KEY_R) && (pressed & KEY_Y) && driveWritable(currentDrive)) {
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
		}

		// Add to selection
		if ((pressed & KEY_L && !(held & KEY_R)) && entry->name != "..") {
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
		}

		if (pressed & KEY_Y) {
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
		}

		if ((pressed & KEY_SELECT) && !clipboardUsed) {
			clipboardOn = !clipboardOn;
		}

		// START menu
		if (pressed & KEY_START) {
			startMenu();
		}

#ifdef SCREENSWAP
		// Swap screens
		if (pressed & KEY_TOUCH) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}
#endif

		// Make a screenshot
		if ((held & KEY_R) && (pressed & KEY_L)) {
			if(screenshot())
				getDirectoryContents(dirContents);
		}
	}
}
