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

#include "main.h"
#include "date.h"
#include "fileOperations.h"
#include "driveMenu.h"
#include "driveOperations.h"

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

using namespace std;

struct DirEntry {
	string name;
	bool isDirectory;
} ;

bool nameEndsWith (const string& name) {

	if (name.size() == 0) return false;

	return true;
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
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			if (dirEntry.name.compare(".") != 0 && (dirEntry.isDirectory || nameEndsWith(dirEntry.name))) {
				dirContents.push_back (dirEntry);
			}

		}
		
		closedir(pdir);
	}	
	
	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

void showDirectoryContents (const vector<DirEntry>& dirContents, int startRow) {
	char path[PATH_MAX];
	
	
	getcwd(path, PATH_MAX);
	
	// Clear the screen
	iprintf ("\x1b[2J");
	
	// Print the path
	if (strlen(path) < SCREEN_COLS) {
		iprintf ("%s", path);
	} else {
		iprintf ("%s", path + strlen(path) - SCREEN_COLS);
	}
	
	// Move to 2nd row
	iprintf ("\x1b[1;0H");
	// Print line of dashes
	iprintf ("--------------------------------");
	
	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);
		char entryName[SCREEN_COLS + 1];
		
		// Set row
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);
		
		if (entry->isDirectory) {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 3] = '\0';
			iprintf (" [%s]", entryName);
		} else {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 1] = '\0';
			iprintf (" %s", entryName);
		}
	}
}

string browseForFile (void) {
	int pressed = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	off_t fileSize = 0;
	vector<DirEntry> dirContents;
	
	getDirectoryContents (dirContents);

	while (true) {
		consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
		DirEntry* entry = &dirContents.at(fileOffset);
		printf (entry->name.c_str());
		printf ("\n");
		if (entry->isDirectory) {
			printf ("(dir)");
		} else {
			fileSize = getFileSize(entry->name.c_str());
			printf ("%i Bytes", (int)fileSize);
		}
		printf ("\x1b[22;0H");
		printf (titleName);
		printf ("\x1b[23;0H");
		printf ("X - DELETE");

		consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);
		showDirectoryContents (dirContents, screenOffset);

		// Clear old cursors
		/*for (int i = ENTRIES_START_ROW; i < ENTRIES_PER_SCREEN + ENTRIES_START_ROW; i++) {
			iprintf ("\x1b[%d;0H ", i);
		}*/
		// Show cursor
		iprintf ("\x1b[%d;0H*", fileOffset - screenOffset + ENTRIES_START_ROW);

		stored_SCFG_MC = REG_SCFG_MC;

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;27H");
			// Print time
			printf (RetTime().c_str());
	
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();

			if (REG_SCFG_MC != stored_SCFG_MC) {
				break;
			}
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_LEFT) && !(pressed & KEY_RIGHT)
				&& !(pressed & KEY_A) && !(pressed & KEY_B) && !(pressed & KEY_X));
	
		iprintf ("\x1b[%d;0H*", fileOffset - screenOffset + ENTRIES_START_ROW);

		if (isDSiMode() && !pressed && dmCursorPosition == 1 && REG_SCFG_MC == 0x11 && flashcardMounted) {
			flashcardUnmount();
			screenMode = 0;
			return "null";
		}

		if (pressed & KEY_UP) 		fileOffset -= 1;
		if (pressed & KEY_DOWN) 	fileOffset += 1;
		if (pressed & KEY_LEFT) 	fileOffset -= ENTRY_PAGE_LENGTH;
		if (pressed & KEY_RIGHT)	fileOffset += ENTRY_PAGE_LENGTH;
		
		if (fileOffset < 0) 	fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
		if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list

		// Scroll screen if needed
		if (fileOffset < screenOffset) 	{
			screenOffset = fileOffset;
			showDirectoryContents (dirContents, screenOffset);
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
			showDirectoryContents (dirContents, screenOffset);
		}
		
		if (pressed & KEY_A) {
			DirEntry* entry = &dirContents.at(fileOffset);
			if (entry->isDirectory) {
				iprintf("Entering directory\n");
				// Enter selected directory
				chdir (entry->name.c_str());
				getDirectoryContents (dirContents);
				screenOffset = 0;
				fileOffset = 0;
			} else {
				applaunch = true;
				// Clear the screen
				iprintf ("\x1b[2J");
				// Return the chosen file
				return entry->name;
			}
		}

		if (pressed & KEY_B) {
			char path[PATH_MAX];
			getcwd(path, PATH_MAX);
			if ((strcmp (path, "sd:/") == 0) || (strcmp (path, "fat:/") == 0)) {
				screenMode = 0;
				return "null";
			}
			// Go up a directory
			chdir ("..");
			getDirectoryContents (dirContents);
			screenOffset = 0;
			fileOffset = 0;
		}

		// Delete file/folder
		if ((pressed & KEY_X) && (strcmp (entry->name.c_str(), "..") != 0)) {
			printf ("\x1b[0;27H");
			printf ("     ");	// Clear time
			consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
			iprintf("Delete \"%s\"?\n", entry->name.c_str());
			printf ("(<A> yes, <B> no)");
			while (true) {
				scanKeys();
				pressed = keysDownRepeat();
				swiWaitForVBlank();
				if (pressed & KEY_A) {
					consoleClear();
					if (entry->isDirectory) {
						printf ("Deleting folder, please wait...");
					} else {
						printf ("Deleting file, please wait...");
					}
					remove(entry->name.c_str());
					getDirectoryContents (dirContents);
					fileOffset--;
					pressed = 0;
					break;
				}
				if (pressed & KEY_B) {
					pressed = 0;
					break;
				}
			}
		}
	}
}
