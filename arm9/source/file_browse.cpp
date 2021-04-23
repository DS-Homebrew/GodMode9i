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
#include <fat.h>

#include "main.h"
#include "date.h"
#include "screenshot.h"
#include "fileOperations.h"
#include "driveMenu.h"
#include "driveOperations.h"
#include "dumpOperations.h"
#include "hexEditor.h"
#include "nitrofs.h"
#include "inifile.h"
#include "nds_loader_arm9.h"

#define SCREEN_COLS 22
#define ENTRIES_PER_SCREEN 23
#define ENTRIES_START_ROW 1
#define OPTIONS_ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10
extern PrintConsole topConsole, bottomConsole;

extern void printBorderTop(void);
extern void printBorderBottom(void);
extern void clearBorderTop(void);
extern void clearBorderBottom(void);
extern void reinitConsoles(void);

static char path[PATH_MAX];

bool extension(const std::string &filename, const std::vector<std::string> &extensions) {
	for(const std::string &ext : extensions) {
		if(filename.length() > ext.length() && strcasecmp(filename.substr(filename.length() - ext.length()).data(), ext.data()) == 0)
			return true;
	}

	return false;
}

void OnKeyPressed(int key) {
	if(key > 0)
		iprintf("%c", key);
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

void getDirectoryContents (std::vector<DirEntry>& dirContents) {
	struct stat st;

	dirContents.clear();

	DIR *pdir = opendir (".");

	if (pdir == NULL) {
		iprintf ("Unable to open the directory.\n");
	} else {

		while(true) {
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if(pent == NULL) break;

			stat(pent->d_name, &st);
			if (strcmp(pent->d_name, "..") != 0) {
				dirEntry.name = pent->d_name;
				dirEntry.isDirectory = st.st_mode & S_IFDIR;
				if (!dirEntry.isDirectory) {
					dirEntry.size = getFileSize(dirEntry.name.c_str());
				}
				if (extension(dirEntry.name, {"nds", "argv", "dsi", "ids", "app", "srl"})) {
					dirEntry.isApp = ((currentDrive == 0 && sdMounted) || (currentDrive == 1 && flashcardMounted));
				} else if (extension(dirEntry.name, {"firm"})) {
					dirEntry.isApp = (isDSiMode() && is3DS && sdMounted);
				} else {
					dirEntry.isApp = false;
				}

				if (dirEntry.name.compare(".") != 0) {
					dirContents.push_back (dirEntry);
				}
			}

		}

		closedir(pdir);
	}

	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);

	DirEntry dirEntry;
	dirEntry.name = "..";	// ".." entry
	dirEntry.isDirectory = true;
	dirEntry.isApp = false;
	dirContents.insert (dirContents.begin(), dirEntry);	// Add ".." to top of list
}

void showDirectoryContents (const std::vector<DirEntry>& dirContents, int fileOffset, int startRow) {
	getcwd(path, PATH_MAX);

	consoleClear();

	// Print the path
	printf ("\x1B[30m");		// Print black color
	// Print time
	printf ("\x1b[0;27H");
	printf (RetTime().c_str());

	printf ("\x1b[0;0H");
	if (strlen(path) < SCREEN_COLS) {
		iprintf ("%s", path);
	} else {
		iprintf ("%s", path + strlen(path) - SCREEN_COLS);
	}

	// Move to 2nd row
	iprintf ("\x1b[1;0H");

	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);

		// Set row
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);
		if ((fileOffset - startRow) == i) {
			printf ("\x1B[47m");		// Print foreground white color
		} else if (entry->selected) {
			printf ("\x1B[33m");		// Print custom yellow color
		} else if (entry->isDirectory) {
			printf ("\x1B[37m");		// Print custom blue color
		} else {
			printf ("\x1B[40m");		// Print foreground black color
		}

		printf ("%.*s", SCREEN_COLS, entry->name.c_str());
		if (entry->name == "..") {
			printf ("\x1b[%d;28H", i + ENTRIES_START_ROW);
			printf ("(..)");
		} else if (entry->isDirectory) {
			printf ("\x1b[%d;27H", i + ENTRIES_START_ROW);
			printf ("(dir)");
		} else {
			printf ("\x1b[%d;23H", i + ENTRIES_START_ROW);
			printBytesAlign((int)entry->size);
		}
	}

	printf ("\x1B[47m");		// Print foreground white color
}

FileOperation fileBrowse_A(DirEntry* entry, char path[PATH_MAX]) {
	int pressed = 0;
	FileOperation assignedOp[4] = {FileOperation::none};
	int optionOffset = 0;
	int cursorScreenPos = 0;
	int maxCursors = -1;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	char fullPath[256];
	snprintf(fullPath, sizeof(fullPath), "%s%s", path, entry->name.c_str());
	printf(fullPath);
	// Position cursor, depending on how long the full file path is
	for (int i = 0; i < 256; i++) {
		if (i == 33 || i == 65 || i == 97 || i == 129 || i == 161 || i == 193 || i == 225) {
			cursorScreenPos++;
		}
		if (fullPath[i] == '\0') {
			break;
		}
	}
	iprintf ("\x1b[%d;0H", cursorScreenPos + OPTIONS_ENTRIES_START_ROW);
	if (!entry->isDirectory) {
		if (entry->isApp) {
			assignedOp[++maxCursors] = FileOperation::bootFile;
			printf("   Boot file (Direct)\n");
			assignedOp[++maxCursors] = FileOperation::bootstrapFile;
			printf("   Bootstrap file\n");
		}
		if(extension(entry->name, {"nds", "dsi", "ids", "app"}))
		{
			assignedOp[++maxCursors] = FileOperation::mountNitroFS;
			printf("   Mount NitroFS\n");
		}
		else if(extension(entry->name, {"sav"}))
		{
			assignedOp[++maxCursors] = FileOperation::restoreSave;
			printf("   Restore save\n");
		}
		else if(extension(entry->name, {"img", "sd"}))
		{
			assignedOp[++maxCursors] = FileOperation::mountImg;
			printf("   Mount as FAT image\n");
		}
		assignedOp[++maxCursors] = FileOperation::hexEdit;
		printf("   Open in hex editor\n");
	}
	assignedOp[++maxCursors] = FileOperation::showInfo;
	printf(entry->isDirectory ? "	Show directory info\n" : "	Show file info\n");
	if (sdMounted && (strcmp(path, "sd:/gm9i/out/") != 0)) {
		assignedOp[++maxCursors] = FileOperation::copySdOut;
		printf("   Copy to sd:/gm9i/out\n");
	}
	if (flashcardMounted && (strcmp(path, "fat:/gm9i/out/") != 0)) {
		assignedOp[++maxCursors] = FileOperation::copyFatOut;
		printf("   Copy to fat:/gm9i/out\n");
	}
	// The bios SHA1 functions are only available on the DSi
	// https://problemkaputt.de/gbatek.htm#biossha1functionsdsionly
	if (isDSiMode()) {
		assignedOp[++maxCursors] = FileOperation::calculateSHA1;
		printf("   Calculate SHA1 hash\n");
	}
	printf("\n(<A> select, <B> cancel)");
	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color
	while (true) {
		// Clear old cursors
		for (int i = OPTIONS_ENTRIES_START_ROW+cursorScreenPos; i < (maxCursors+1) + OPTIONS_ENTRIES_START_ROW+cursorScreenPos; i++) {
			iprintf ("\x1b[%d;0H  ", i);
		}
		// Show cursor
		iprintf ("\x1b[%d;0H->", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);

		consoleSelect(&topConsole);
		printf ("\x1B[30m");		// Print black color for time text
		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf (" %s" ,RetTime().c_str());

			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN)
				&& !(pressed & KEY_A) && !(pressed & KEY_B)
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

		consoleSelect(&bottomConsole);
		printf ("\x1B[47m");		// Print foreground white color

		if (pressed & KEY_UP)		optionOffset -= 1;
		if (pressed & KEY_DOWN)		optionOffset += 1;

		if (optionOffset < 0)				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			switch(assignedOp[optionOffset]) {
				case FileOperation::bootFile: {
					applaunch = true;
					iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
					printf("Now loading...");
					break;
				} case FileOperation::bootstrapFile: {
					char baseFile[256], savePath[PATH_MAX]; //, bootstrapConfigPath[32];
					//snprintf(bootstrapConfigPath, 32, "%s:/_nds/nds-bootstrap.ini", isDSiMode() ? "sd" : "fat");
					strncpy(baseFile, entry->name.c_str(), 256);
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
				} case FileOperation::restoreSave: {
					ndsCardSaveRestore(entry->name.c_str());
					break;
				} case FileOperation::copySdOut: {
					if (access("sd:/gm9i", F_OK) != 0) {
						iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
						printf("Creating directory...");
						mkdir("sd:/gm9i", 0777);
					}
					if (access("sd:/gm9i/out", F_OK) != 0) {
						iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
						printf("Creating directory...");
						mkdir("sd:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "sd:/gm9i/out/%s", entry->name.c_str());
					iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
					printf("Copying...        	 ");
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
						iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
						printf("Creating directory...");
						mkdir("fat:/gm9i", 0777);
					}
					if (access("fat:/gm9i/out", F_OK) != 0) {
						iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
						printf("Creating directory...");
						mkdir("fat:/gm9i/out", 0777);
					}
					char destPath[256];
					snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s", entry->name.c_str());
					iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
					printf("Copying...        	 ");
					remove(destPath);
					char sourceFolder[PATH_MAX];
					getcwd(sourceFolder, PATH_MAX);
					char sourcePath[PATH_MAX];
					snprintf(sourcePath, sizeof(sourcePath), "%s%s", sourceFolder, entry->name.c_str());
					fcopy(sourcePath, destPath);
					chdir(sourceFolder);	// For after copying a folder
					break;
				} case FileOperation::mountNitroFS: {
					nitroMounted = nitroFSInit(entry->name.c_str());
					if (nitroMounted) {
						chdir("nitro:/");
						nitroCurrentDrive = currentDrive;
						currentDrive = 5;
					}
					break;
				} case FileOperation::showInfo: {
					changeFileAttribs(entry);
					break;
				} case FileOperation::mountImg: {
					imgMounted = imgMount(entry->name.c_str());
					if (imgMounted) {
						chdir("img:/");
						imgCurrentDrive = currentDrive;
						currentDrive = 6;
					}
					break;
				} case FileOperation::hexEdit: {
					hexEditor(entry->name.c_str(), currentDrive);
				} case FileOperation::calculateSHA1: {
					iprintf("\x1b[2J");
					iprintf("Calculating SHA1 hash of:\n%s\n", entry->name.c_str());
					iprintf("Press <START> to cancel\n\n");
					u8 sha1[20] = {0};
					bool ret = calculateSHA1(strcat(getcwd(path, PATH_MAX), entry->name.c_str()), sha1);
					if (!ret) break;
					iprintf("SHA1 hash is: \n");
					for (int i = 0; i < 20; ++i) iprintf("%02X", sha1[i]);
					consoleSelect(&topConsole);
					iprintf ("\x1B[30m");           // Print black color
					// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
					int pressed;
					do {
						// Move to right side of screen
						iprintf ("\x1b[0;26H");
						// Print time
						iprintf (" %s" ,RetTime().c_str());
						scanKeys();
						pressed = keysDownRepeat();
						swiWaitForVBlank();
					} while (!(pressed & (KEY_A | KEY_Y | KEY_B | KEY_X)));
					break;
				} case FileOperation::none: {
					break;
				}
			}
			return assignedOp[optionOffset];
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
	}
}

bool fileBrowse_paste(char dest[256]) {
	int pressed = 0;
	int optionOffset = 0;
	int maxCursors = -1;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf("Paste clipboard here?");
	printf("\n\n");
	iprintf ("\x1b[%d;0H", OPTIONS_ENTRIES_START_ROW);
	maxCursors++;
	printf("   Copy files\n");
	for (auto &file : clipboard) {
		if (file.nitro)
			continue;
		maxCursors++;
		printf("   Move files\n");
		break;
	}
	printf("\n");
	printf("(<A> select, <B> cancel)");
	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color
	while (true) {
		// Clear old cursors
		for (int i = OPTIONS_ENTRIES_START_ROW; i < (maxCursors+1) + OPTIONS_ENTRIES_START_ROW; i++) {
			iprintf ("\x1b[%d;0H  ", i);
		}
		// Show cursor
		iprintf ("\x1b[%d;0H->", optionOffset + OPTIONS_ENTRIES_START_ROW);

		consoleSelect(&topConsole);
		printf ("\x1B[30m");		// Print black color for time text
		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf (" %s" ,RetTime().c_str());

			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN)
				&& !(pressed & KEY_A) && !(pressed & KEY_B)
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);


		consoleSelect(&bottomConsole);
		printf ("\x1B[47m");		// Print foreground white color

		if (pressed & KEY_UP)		optionOffset -= 1;
		if (pressed & KEY_DOWN)		optionOffset += 1;

		if (optionOffset < 0)				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW);
			printf(optionOffset ? "Moving... " : "Copying...");
			for (auto &file : clipboard) {
				std::string destPath = dest + file.name;
				if (file.path == destPath)
					continue;	// If the source and destination for the clipped file is the same skip it

				if (optionOffset && !file.nitro ) {	 // Don't remove if from nitro
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
	DirEntry *entry = NULL;
	chdir (path);
	getDirectoryContents(dirContents);
	for (int i = 1; i < ((int)dirContents.size()); i++) {
		entry = &dirContents.at(i);
		if (entry->isDirectory)	recRemove(entry->name.c_str(), dirContents);
		if (!(FAT_getAttr(entry->name.c_str()) & ATTR_READONLY)) {
			remove(entry->name.c_str());
		}
	}
	chdir ("..");
	remove(path);
}

void fileBrowse_drawBottomScreen(DirEntry* entry) {
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf ("\x1b[22;0H");
	printf ("%s\n", titleName);
	printf ("X - DELETE/[+R] RENAME file\n");
	printf ("L - %s files (with \x18\x19\x1A\x1B)\n", entry->selected ? "DESELECT" : "SELECT");
	printf ("Y - %s file/[+R] CREATE entry%s", clipboardOn ? "PASTE" : "COPY", clipboardOn ? "" : "\n");
	printf ("R+A - Directory options\n");
	if (sdMounted || flashcardMounted) {
		printf ("%s\n", SCREENSHOTTEXT);
	}
	printf ("%s\n", clipboardOn ? "SELECT - Clear Clipboard" : "SELECT - Restore Clipboard");
	if (!isDSiMode() && isRegularDS) {
		printf (POWERTEXT_DS);
	} else if (is3DS) {
		printf ("%s\n%s", POWERTEXT_3DS, HOMETEXT);
	} else {
		printf (POWERTEXT);
	}
	printf (entry->selected ? "\x1B[33m" : (entry->isDirectory ? "\x1B[37m" : "\x1B[40m"));		// Print custom blue color or foreground black color
	printf ("\x1b[0;0H");
	printf ("%s\n", entry->name.c_str());
	if (entry->name != "..") {
		if (entry->isDirectory) {
			printf ("(dir)");
		} else if (entry->size == 1) {
			printf ("%i Byte", (int)entry->size);
		} else {
			printf ("%i Bytes", (int)entry->size);
		}
	}
	if (clipboardOn) {
		printf ("\x1b[9;0H");
		printf ("\x1B[47m");		// Print foreground white color
		printf ("[CLIPBOARD]\n");
		for (size_t i = 0; i < clipboard.size(); ++i) {
			printf (clipboard[i].folder ? "\x1B[37m" : "\x1B[40m");		// Print custom blue color or foreground black color
			if (i < 4) {
				printf ("%s\n", clipboard[i].name.c_str());
			} else {
				printf ("%d more files...\n", clipboard.size() - 4);
				break;
			}
		}
	}
}

std::string browseForFile (void) {
	int pressed = 0;
	int held = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	std::vector<DirEntry> dirContents;

	getDirectoryContents (dirContents);

	while (true) {
		DirEntry* entry = &dirContents.at(fileOffset);

		consoleSelect(&bottomConsole);
		fileBrowse_drawBottomScreen(entry);
		consoleSelect(&topConsole);
		showDirectoryContents (dirContents, fileOffset, screenOffset);

		stored_SCFG_MC = REG_SCFG_MC;

		printf ("\x1B[30m");		// Print black color for time text

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf (" %s" ,RetTime().c_str());

			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();

			if (REG_SCFG_MC != stored_SCFG_MC) {
				break;
			}

			if ((held & KEY_R) && (pressed & KEY_L)) {
				break;
			}
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_LEFT) && !(pressed & KEY_RIGHT)
				&& !(pressed & KEY_A) && !(pressed & KEY_B) && !(pressed & KEY_X) && !(pressed & KEY_Y)
				&& !(pressed & KEY_L) && !(pressed & KEY_SELECT)
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

		printf ("\x1B[47m");		// Print foreground white color
		iprintf ("\x1b[%d;0H", fileOffset - screenOffset + ENTRIES_START_ROW);

		if (isDSiMode() && !pressed && currentDrive == 1 && REG_SCFG_MC == 0x11 && flashcardMounted) {
			flashcardUnmount();
			screenMode = 0;
			return "null";
		}

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
				iprintf("Entering directory ");
				// Enter selected directory
				chdir (entry->name.c_str());
				getDirectoryContents (dirContents);
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
		if ((held & KEY_R) && (pressed & KEY_X) && (entry->name != ".." && strncmp(path, "nitro:/", 7) != 0)) {
			printf ("\x1b[0;27H");
			printf ("     ");	// Clear time
			pressed = 0;
			consoleDemoInit();
			Keyboard *kbd = keyboardDemoInit();
			char newName[256];
			kbd->OnKeyPressed = OnKeyPressed;

			keyboardShow();
			printf("Rename to: \n");
			fgets(newName, 256, stdin);
			newName[strlen(newName)-1] = 0;
			keyboardHide();
			consoleClear();

			reinitConsoles();

			if (newName[0] != '\0') {
				// Check for unsupported characters
				for (int i = 0; i < (int)sizeof(newName); i++) {
					if (newName[i] == '>'
					|| newName[i] == '<'
					|| newName[i] == ':'
					|| newName[i] == '"'
					|| newName[i] == '/'
					|| newName[i] == '\x5C'
					|| newName[i] == '|'
					|| newName[i] == '?'
					|| newName[i] == '*')
					{
						newName[i] = '_';	// Remove unsupported character
					}
				}
				if (rename(entry->name.c_str(), newName) == 0) {
					getDirectoryContents (dirContents);
				}
			}
		}

		// Delete action
		if ((pressed & KEY_X) && (entry->name != ".." && strncmp(path, "nitro:/", 7) != 0)) {
			consoleSelect(&bottomConsole);
			consoleClear();
			printf ("\x1B[47m");		// Print foreground white color
			int selections = std::count_if(dirContents.begin(), dirContents.end(), [](const DirEntry &x){ return x.selected; });
			if (entry->selected && selections > 1) {
				iprintf("Delete %d paths?\n", selections);
				for (uint i = 0, printed = 0; i < dirContents.size() && printed < 5; i++) {
					if (dirContents[i].selected) {
						iprintf("\x1B[41m- %s\n", dirContents[i].name.c_str());
						printed++;
					}
				}
				if(selections > 5)
					iprintf("\x1B[41m- and %d more...\n", selections - 5);
				iprintf("\x1B[47m");
			} else {
				iprintf("Delete \"%s\"?\n", entry->name.c_str());
			}
			printf ("(<A> yes, <B> no)");
			consoleSelect(&topConsole);
			printf ("\x1B[30m");		// Print black color for time text
			while (true) {
				// Move to right side of screen
				printf ("\x1b[0;26H");
				// Print time
				printf (" %s" ,RetTime().c_str());

				scanKeys();
				pressed = keysDownRepeat();
				swiWaitForVBlank();
				if (pressed & KEY_A) {
					consoleSelect(&bottomConsole);
					consoleClear();
					printf ("\x1B[47m");		// Print foreground white color
					if (entry->selected) {
						printf ("Deleting files, please wait...");
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
						printf ("Failed deleting:\n");
						printf (entry->name.c_str());
						printf ("\n");
						printf ("\n");
						printf ("(<A> to continue)");
						pressed = 0;
						consoleSelect(&topConsole);
						printf ("\x1B[30m");		// Print black color for time text
						while (!(pressed & KEY_A)) {
							// Move to right side of screen
							printf ("\x1b[0;26H");
							// Print time
							printf (" %s" ,RetTime().c_str());

							scanKeys();
							pressed = keysDown();
							swiWaitForVBlank();
						}
						for (int i = 0; i < 15; i++) swiWaitForVBlank();
					} else {
						if (entry->isDirectory) {
							printf ("Deleting folder, please wait...");
							recRemove(entry->name.c_str(), dirContents);
						} else {
							printf ("Deleting file, please wait...");
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
		if ((held & KEY_R) && (pressed & KEY_Y) && (strncmp(path, "nitro:/", 7) != 0)) {
			printf ("\x1b[0;27H");
			printf ("     ");	// Clear time
			pressed = 0;
			consoleDemoInit();
			Keyboard *kbd = keyboardDemoInit();
			char newName[256];
			kbd->OnKeyPressed = OnKeyPressed;

			keyboardShow();
			printf("Name for new folder: \n");
			fgets(newName, 256, stdin);
			newName[strlen(newName)-1] = 0;
			keyboardHide();
			consoleClear();

			reinitConsoles();

			if (newName[0] != '\0') {
				// Check for unsupported characters
				for (int i = 0; i < (int)sizeof(newName); i++) {
					if (newName[i] == '>'
					|| newName[i] == '<'
					|| newName[i] == ':'
					|| newName[i] == '"'
					|| newName[i] == '/'
					|| newName[i] == '\x5C'
					|| newName[i] == '|'
					|| newName[i] == '?'
					|| newName[i] == '*')
					{
						newName[i] = '_';	// Remove unsupported character
					}
				}
				if (mkdir(newName, 0777) == 0) {
					getDirectoryContents (dirContents);
				}
			}
		}

		// Add to selection
		if (pressed & KEY_L && entry->name != "..") {
			bool select = !entry->selected;
			entry->selected = select;
			while(held & KEY_L) {
				do {
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print black color for time text
					printf ("\x1B[30m");
					// Print time
					printf (" %s" ,RetTime().c_str());

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

				consoleSelect(&bottomConsole);
				fileBrowse_drawBottomScreen(entry);
				consoleSelect(&topConsole);
				showDirectoryContents (dirContents, fileOffset, screenOffset);
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
								clipboard.emplace_back(path + item.name, item.name, item.isDirectory, currentDrive, !strncmp(path, "nitro:/", 7));
								item.selected = false;
							}
						}
					} else {
						clipboard.emplace_back(path + entry->name, entry->name, entry->isDirectory, currentDrive, !strncmp(path, "nitro:/", 7));
					}
				}
			// Paste
			} else if (strncmp(path, "nitro:/", 7) != 0 && fileBrowse_paste(path)) {
				getDirectoryContents (dirContents);
			}
		}

		if ((pressed & KEY_SELECT) && !clipboardUsed) {
			clipboardOn = !clipboardOn;
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
		  if (sdMounted || flashcardMounted) {
			if (access((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), F_OK) != 0) {
				mkdir((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), 0777);
				if (strcmp(path, (sdMounted ? "sd:/" : "fat:/")) == 0) {
					getDirectoryContents (dirContents);
				}
			}
			if (access((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), F_OK) != 0) {
				mkdir((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), 0777);
				if (strcmp(path, (sdMounted ? "sd:/gm9i/" : "fat:/gm9i/")) == 0) {
					getDirectoryContents (dirContents);
				}
			}
			char timeText[8];
			snprintf(timeText, sizeof(timeText), "%s", RetTime().c_str());
			char fileTimeText[8];
			snprintf(fileTimeText, sizeof(fileTimeText), "%s", RetTimeForFilename().c_str());
			char snapPath[40];
			// Take top screenshot
			snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_top.bmp", (sdMounted ? "sd" : "fat"), fileTimeText);
			screenshotbmp(snapPath);
			// Seamlessly swap top and bottom screens
			lcdMainOnBottom();
			printBorderBottom();
			consoleSelect(&bottomConsole);
			showDirectoryContents (dirContents, fileOffset, screenOffset);
			printf("\x1B[30m");		// Print black color for time text
			printf ("\x1b[0;26H");
			printf (" %s" ,timeText);
			clearBorderTop();
			consoleSelect(&topConsole);
			fileBrowse_drawBottomScreen(entry);
			// Take bottom screenshot
			snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_bot.bmp", (sdMounted ? "sd" : "fat"), fileTimeText);
			screenshotbmp(snapPath);
			if (strcmp(path, (sdMounted ? "sd:/gm9i/out/" : "fat:/gm9i/out/")) == 0) {
				getDirectoryContents (dirContents);
			}
			lcdMainOnTop();
			printBorderTop();
			clearBorderBottom();
		  }
		}
	}
}
