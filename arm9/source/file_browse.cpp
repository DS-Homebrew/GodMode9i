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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

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
#include "nitrofs.h"

#define SCREEN_COLS 22
#define ENTRIES_PER_SCREEN 23
#define ENTRIES_START_ROW 1
#define OPTIONS_ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10
bool bigJump = false;
extern PrintConsole topConsole, bottomConsole;

extern void printBorderTop(void);
extern void printBorderBottom(void);
extern void clearBorderTop(void);
extern void clearBorderBottom(void);
extern void reinitConsoles(void);

static char path[PATH_MAX];

bool nameEndsWith (const string& name) {

	if (name.size() == 0) return false;

	return true;
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

void getDirectoryContents (vector<DirEntry>& dirContents) {
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
				dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;
				if (!dirEntry.isDirectory) {
					dirEntry.size = getFileSize(dirEntry.name.c_str());
				}
				if ((dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "nds")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "NDS")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "argv")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "ARGV")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "dsi")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "DSI")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "ids")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "IDS")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "app")
				|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "APP"))
				{
					dirEntry.isApp = ((currentDrive == 0 && sdMounted) || (currentDrive == 1 && flashcardMounted));
				} else if ((dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "firm")
						|| (dirEntry.name.substr(dirEntry.name.find_last_of(".") + 1) == "FIRM"))
				{
					dirEntry.isApp = (isDSiMode() && is3DS && sdMounted);
				} else {
					dirEntry.isApp = false;
				}

				if (dirEntry.name.compare(".") != 0 && (dirEntry.isDirectory || nameEndsWith(dirEntry.name))) {
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

void showDirectoryContents (const vector<DirEntry>& dirContents, int fileOffset, int startRow) {
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
		} else if (entry->isDirectory) {
			printf ("\x1B[37m");		// Print custom blue color
		} else {
			printf ("\x1B[40m");		// Print foreground black color
		}

		printf ("%.*s", SCREEN_COLS, entry->name.c_str());
		if (strcmp(entry->name.c_str(), "..") == 0) {
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

int fileBrowse_A(DirEntry* entry, char path[PATH_MAX]) {
	int pressed = 0;
	int assignedOp[4] = {0};
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
			maxCursors++;
			assignedOp[maxCursors] = 0;
			printf("   Boot file\n");
		}
		if((entry->name.substr(entry->name.find_last_of(".") + 1) == "nds")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "NDS")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "dsi")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "DSI")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "ids")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "IDS")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "app")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "APP"))
		{
			maxCursors++;
			assignedOp[maxCursors] = 3;
			printf("   Mount NitroFS\n");
		}
		else
		if((entry->name.substr(entry->name.find_last_of(".") + 1) == "img")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "IMG")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "sd")
		|| (entry->name.substr(entry->name.find_last_of(".") + 1) == "SD"))
		{
			maxCursors++;
			assignedOp[maxCursors] = 5;
			printf("   Mount as FAT image\n");
		}
	}
	maxCursors++;
	assignedOp[maxCursors] = 4;
	printf(entry->isDirectory ? "   Show directory info\n" : "   Show file info\n");
	if (sdMounted && (strcmp (path, "sd:/gm9i/out/") != 0)) {
		maxCursors++;
		assignedOp[maxCursors] = 1;
		printf("   Copy to sd:/gm9i/out\n");
	}
	if (flashcardMounted && (strcmp (path, "fat:/gm9i/out/") != 0)) {
		maxCursors++;
		assignedOp[maxCursors] = 2;
		printf("   Copy to fat:/gm9i/out\n");
	}
	printf("\n");
	printf("(<A> select, <B> cancel)");
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
				&& !(pressed & KEY_A) && !(pressed & KEY_B));

		consoleSelect(&bottomConsole);
		printf ("\x1B[47m");		// Print foreground white color

		if (pressed & KEY_UP) 		optionOffset -= 1;
		if (pressed & KEY_DOWN) 	optionOffset += 1;
		
		if (optionOffset < 0) 				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			if (assignedOp[optionOffset] == 0) {
				applaunch = true;
				iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW+cursorScreenPos);
				printf("Now loading...");
			} else if (assignedOp[optionOffset] == 1) {
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
				printf("Copying...           ");
				remove(destPath);
				fcopy(entry->name.c_str(), destPath);
			} else if (assignedOp[optionOffset] == 2) {
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
				printf("Copying...           ");
				remove(destPath);
				fcopy(entry->name.c_str(), destPath);
			} else if (assignedOp[optionOffset] == 3) {
				nitroMounted = nitroFSInit(entry->name.c_str());
				if (nitroMounted) {
					chdir("nitro:/");
					nitroCurrentDrive = currentDrive;
					currentDrive = 5;
				}
			} else if (assignedOp[optionOffset] == 4) {
				changeFileAttribs(entry);
			} else if (assignedOp[optionOffset] == 5) {
				imgMounted = imgMount(entry->name.c_str());
				if (imgMounted) {
					chdir("img:/");
					imgCurrentDrive = currentDrive;
					currentDrive = 6;
				}
			}
			return assignedOp[optionOffset];
		}
		if (pressed & KEY_B) {
			return -1;
		}
	}
}

bool fileBrowse_paste(char destPath[256]) {
	int pressed = 0;
	int optionOffset = 0;
	int maxCursors = -1;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf(clipboardFolder ? "Paste folder here?" : "Paste file here?");
	printf("\n\n");
	iprintf ("\x1b[%d;0H", OPTIONS_ENTRIES_START_ROW);
	maxCursors++;
	printf("   Copy path\n");
	if (!clipboardInNitro) {
		maxCursors++;
		printf("   Move path\n");
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
				&& !(pressed & KEY_A) && !(pressed & KEY_B));

		consoleSelect(&bottomConsole);
		printf ("\x1B[47m");		// Print foreground white color

		if (pressed & KEY_UP) 		optionOffset -= 1;
		if (pressed & KEY_DOWN) 	optionOffset += 1;
		
		if (optionOffset < 0) 				optionOffset = maxCursors;		// Wrap around to bottom of list
		if (optionOffset > maxCursors)		optionOffset = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			iprintf ("\x1b[%d;3H", optionOffset + OPTIONS_ENTRIES_START_ROW);
			if (optionOffset == 0) {
				printf("Copying...");
				remove(destPath);
				fcopy(clipboard, destPath);
			} else {
				printf("Moving...");
				if (currentDrive == clipboardDrive) {
					rename(clipboard, destPath);
				} else {
					fcopy(clipboard, destPath);		// Copy file to destination, since renaming won't work
					remove(clipboard);				// Delete source file after copying
				}
				clipboardUsed = false;		// Disable clipboard restore
			}
			clipboardOn = false;	// Clear clipboard after copying or moving
			return true;
		}
		if (pressed & KEY_B) {
			return false;
		}
	}
}

void recRemove(DirEntry* entry, std::vector<DirEntry> dirContents) {
	DirEntry* startEntry = entry;
	chdir (entry->name.c_str());
	getDirectoryContents(dirContents);
	for (int i = 1; i < ((int)dirContents.size()); i++) {
		entry = &dirContents.at(i);
		if (entry->isDirectory)	recRemove(entry, dirContents);
		if (!(FAT_getAttr(entry->name.c_str()) & ATTR_READONLY)) {
			remove(entry->name.c_str());
		}
	}
	chdir ("..");
	remove(startEntry->name.c_str());
}

void fileBrowse_drawBottomScreen(DirEntry* entry) {
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf ("\x1b[22;0H");
	printf ("%s\n", titleName);
	printf ("X - DELETE/[+R] RENAME file\n");
	printf ("%s/[+R] CREATE entry%s", clipboardOn ? "Y - PASTE file" : "Y - COPY file", clipboardOn ? "" : "\n");
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

	printf (entry->isDirectory ? "\x1B[37m" : "\x1B[40m");		// Print custom blue color or foreground black color
	printf ("\x1b[0;0H");
	printf ("%s\n", entry->name.c_str());
	if (strcmp(entry->name.c_str(), "..") != 0) {
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
		printf (clipboardFolder ? "\x1B[37m" : "\x1B[40m");		// Print custom blue color or foreground black color
		printf (clipboardFilename);
	}
}

string browseForFile (void) {
	int pressed = 0;
	int held = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	vector<DirEntry> dirContents;
	
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
				&& !(pressed & KEY_SELECT));
	
		printf ("\x1B[47m");		// Print foreground white color
		iprintf ("\x1b[%d;0H", fileOffset - screenOffset + ENTRIES_START_ROW);

		if (isDSiMode() && !pressed && currentDrive == 1 && REG_SCFG_MC == 0x11 && flashcardMounted) {
			flashcardUnmount();
			screenMode = 0;
			return "null";
		}

		if (pressed & KEY_UP) {		fileOffset -= 1; bigJump = false;  }
		if (pressed & KEY_DOWN) {	fileOffset += 1; bigJump = false; }
		if (pressed & KEY_LEFT) {	fileOffset -= ENTRY_PAGE_LENGTH; bigJump = true; }
		if (pressed & KEY_RIGHT) {	fileOffset += ENTRY_PAGE_LENGTH; bigJump = true; }
		
		if ((fileOffset < 0) & (bigJump == false))	fileOffset = dirContents.size() - 1;	// Wrap around to bottom of list (UP press)
		else if ((fileOffset < 0) & (bigJump == true))	fileOffset = 0;		// Move to bottom of list (RIGHT press)
		if ((fileOffset > ((int)dirContents.size() - 1)) & (bigJump == false))	fileOffset = 0;		// Wrap around to top of list (DOWN press)
		else if ((fileOffset > ((int)dirContents.size() - 1)) & (bigJump == true))	fileOffset = dirContents.size() - 1;	// Move to top of list (LEFT press)


		// Scroll screen if needed
		if (fileOffset < screenOffset) 	{
			screenOffset = fileOffset;
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
		}

		getcwd(path, PATH_MAX);

		if ((!(held & KEY_R) && (pressed & KEY_A))
		|| (!entry->isDirectory && (held & KEY_R) && (pressed & KEY_A))) {
			if ((strcmp (entry->name.c_str(), "..") == 0) && (strcmp (path, getDrivePath()) == 0))
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
				int getOp = fileBrowse_A(entry, path);
				if (getOp == 0) {
					// Return the chosen file
					return entry->name;
				} else if (getOp == 1 || getOp == 2 || (getOp == 3 && nitroMounted) || (getOp == 5 && imgMounted)) {
					getDirectoryContents (dirContents);		// Refresh directory listing
					if ((getOp == 3 && nitroMounted) || (getOp == 5 && imgMounted)) {
						screenOffset = 0;
						fileOffset = 0;
					}
				} else if (getOp == 4) {
					for (int i = 0; i < 15; i++) swiWaitForVBlank();
				}
			}
		}

		// Directory options
		if (entry->isDirectory && (held & KEY_R) && (pressed & KEY_A)) {
			if (strcmp(entry->name.c_str(), "..") == 0) {
				screenMode = 0;
				return "null";
			} else {
				int getOp = fileBrowse_A(entry, path);
				if (getOp == 1 || getOp == 2) {
					getDirectoryContents (dirContents);		// Refresh directory listing
					if (getOp == 3 && nitroMounted) {
						screenOffset = 0;
						fileOffset = 0;
					}
				} else if (getOp == 4) {
					for (int i = 0; i < 15; i++) swiWaitForVBlank();
				}
			}
		}

		if (pressed & KEY_B) {
			if (strcmp (path, getDrivePath()) == 0) {
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
		if ((held & KEY_R) && (pressed & KEY_X) && (strcmp (entry->name.c_str(), "..") != 0) && (strncmp (path, "nitro:/", 7) != 0)) {
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

		// Delete file/folder
		if ((pressed & KEY_X) && (strcmp (entry->name.c_str(), "..") != 0) && (strncmp (path, "nitro:/", 7) != 0)) {
			consoleSelect(&bottomConsole);
			consoleClear();
			printf ("\x1B[47m");		// Print foreground white color
			iprintf("Delete \"%s\"?\n", entry->name.c_str());
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
					if (FAT_getAttr(entry->name.c_str()) & ATTR_READONLY) {
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
							recRemove(entry, dirContents);
						} else {
							printf ("Deleting file, please wait...");
							remove(entry->name.c_str());
						}
						char filePath[256];
						snprintf(filePath, sizeof(filePath), "%s%s", path, entry->name.c_str());
						if (strcmp(filePath, clipboard) == 0) {
							clipboardUsed = false;	// Disable clipboard restore
							clipboardOn = false;
						}
						getDirectoryContents (dirContents);
						fileOffset--;
					}
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
		if ((held & KEY_R) && (pressed & KEY_Y) && (strncmp (path, "nitro:/", 7) != 0)) {
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

		// Copy file/folder
		if (pressed & KEY_Y) {
			if (clipboardOn) {
				char destPath[256];
				snprintf(destPath, sizeof(destPath), "%s%s", path, clipboardFilename);
				if (strncmp (path, "nitro:/", 7) != 0 && string(clipboard) != string(destPath)) {
					if (fileBrowse_paste(destPath)) {
						getDirectoryContents (dirContents);
					}
				}
			} else if (strcmp(entry->name.c_str(), "..") != 0) {
				snprintf(clipboard, sizeof(clipboard), "%s%s", path, entry->name.c_str());
				snprintf(clipboardFilename, sizeof(clipboardFilename), "%s", entry->name.c_str());
				clipboardFolder = entry->isDirectory;
				clipboardOn = true;
				clipboardDrive = currentDrive;
				clipboardInNitro = (strncmp (path, "nitro:/", 7) == 0);
				clipboardUsed = true;
			}
		}

		if ((pressed & KEY_SELECT) && clipboardUsed) {
			clipboardOn = !clipboardOn;
		}

		// Make a screenshot
		if ((held & KEY_R) && (pressed & KEY_L)) {
		  if (sdMounted || flashcardMounted) {
			if (access((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), F_OK) != 0) {
				mkdir((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), 0777);
				if (strcmp (path, (sdMounted ? "sd:/" : "fat:/")) == 0) {
					getDirectoryContents (dirContents);
				}
			}
			if (access((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), F_OK) != 0) {
				mkdir((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), 0777);
				if (strcmp (path, (sdMounted ? "sd:/gm9i/" : "fat:/gm9i/")) == 0) {
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
			if (strcmp (path, (sdMounted ? "sd:/gm9i/out/" : "fat:/gm9i/out/")) == 0) {
				getDirectoryContents (dirContents);
			}
			lcdMainOnTop();
			printBorderTop();
			clearBorderBottom();
		  }
		}
	}
}
