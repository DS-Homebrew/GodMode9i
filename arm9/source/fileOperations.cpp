#include "fileOperations.h"
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <dirent.h>
#include <vector>

#include "date.h"
#include "file_browse.h"

using namespace std;

#define copyBufSize 0x8000

u32 copyBuf[copyBufSize];

extern PrintConsole topConsole, bottomConsole;

char clipboard[256];
char clipboardFilename[256];
bool clipboardFolder = false;
bool clipboardOn = false;
bool clipboardUsed = false;
int clipboardDrive = false;	// 0 == SD card, 1 == Flashcard, 2 == RAMdrive 1, 3 == RAMdrive 2
bool clipboardInNitro = false;

void printBytes(int bytes)
{
	if (bytes == 1)
		iprintf("%4d Byte", bytes);

	else if (bytes < 1024)
		iprintf("%3d Bytes", bytes);

	else if (bytes < 1024 * 1024)
		printf("%6d KB", bytes / 1024);

	else if (bytes < 1024 * 1024 * 1024)
		printf("%6d MB", bytes / 1024 / 1024);

	else
		printf("%6d GB", bytes / 1024 / 1024 / 1024);
}

off_t getFileSize(const char *fileName)
{
    FILE* fp = fopen(fileName, "rb");
    off_t fsize = 0;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);			// Get source file's size
		fseek(fp, 0, SEEK_SET);
	}
	fclose(fp);

	return fsize;
}

void dirCopy(DirEntry* entry, int i, const char *destinationPath, const char *sourcePath) {
	vector<DirEntry> dirContents;
	dirContents.clear();
	if (entry->isDirectory)	chdir((sourcePath + ("/" + entry->name)).c_str());
	getDirectoryContents(dirContents);
	if (((int)dirContents.size()) == 1)	mkdir((destinationPath + ("/" + entry->name)).c_str(), 0777);
	if (((int)dirContents.size()) != 1)	fcopy((sourcePath + ("/" + entry->name)).c_str(), (destinationPath + ("/" + entry->name)).c_str());
}

int fcopy(const char *sourcePath, const char *destinationPath)
{
	DIR *isDir = opendir (sourcePath);
	
	if (isDir != NULL) {
		closedir(isDir);

		// Source path is a directory
		chdir(sourcePath);
		vector<DirEntry> dirContents;
		getDirectoryContents(dirContents);
		DirEntry* entry = &dirContents.at(1);

		mkdir(destinationPath, 0777);
		for (int i = 1; i < ((int)dirContents.size()); i++) {
			chdir(sourcePath);
			entry = &dirContents.at(i);
			dirCopy(entry, i, destinationPath, sourcePath);
		}

		chdir (destinationPath);
		chdir ("..");
		return 1;
	} else {
		closedir(isDir);

		// Source path is a file
	    FILE* sourceFile = fopen(sourcePath, "rb");
	    off_t fsize = 0;
	    if (sourceFile) {
	        fseek(sourceFile, 0, SEEK_END);
	        fsize = ftell(sourceFile);			// Get source file's size
			fseek(sourceFile, 0, SEEK_SET);
		} else {
			fclose(sourceFile);
			return -1;
		}

	    FILE* destinationFile = fopen(destinationPath, "wb");
		//if (destinationFile) {
			fseek(destinationFile, 0, SEEK_SET);
		/*} else {
			fclose(sourceFile);
			fclose(destinationFile);
			return -1;
		}*/

		off_t offset = 0;
		int numr;
		while (1)
		{
			scanKeys();
			if (keysHeld() & KEY_B) {
				// Cancel copying
				fclose(sourceFile);
				fclose(destinationFile);
				return -1;
				break;
			}
			consoleSelect(&topConsole);
			printf ("\x1B[30m");		// Print black color
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf (" %s" ,RetTime().c_str());

			consoleSelect(&bottomConsole);
			printf ("\x1B[47m");		// Print foreground white color
			printf ("\x1b[16;0H");
			printf ("Progress:\n");
			printf ("%i/%i Bytes                       ", (int)offset, (int)fsize);

			// Copy file to destination path
			numr = fread(copyBuf, 1, copyBufSize, sourceFile);
			fwrite(copyBuf, 1, numr, destinationFile);
			offset += copyBufSize;

			if (offset > fsize) {
				fclose(sourceFile);
				fclose(destinationFile);

				printf ("\x1b[17;0H");
				printf ("%i/%i Bytes                       ", (int)fsize, (int)fsize);
				for (int i = 0; i < 30; i++) swiWaitForVBlank();

				return 1;
				break;
			}
		}

		return -1;
	}
}

void changeFileAttribs(DirEntry* entry) {
	consoleClear();
	int pressed = 0;
	int cursorScreenPos = 0;
	uint8_t currentAttribs = FAT_getAttr(entry->name.c_str());
	uint8_t newAttribs = currentAttribs;

	// Position cursor, depending on how long the file name is
	for (int i = 0; i < 256; i++) {
		if (i == 33 || i == 65 || i == 97 || i == 129 || i == 161 || i == 193 || i == 225) {
			cursorScreenPos++;
		}
		if (entry->name.c_str()[i] == '\0') {
			break;
		}
	}

	printf ("\x1b[0;0H");
	printf (entry->name.c_str());
	if (!entry->isDirectory) {
		printf ("\x1b[%i;0H", 3+cursorScreenPos);
		printf ("filesize: ");
		printBytes(entry->size);
	}
	printf ("\x1b[%i;0H", 5+cursorScreenPos);
	printf ("[ ] U read-only  [ ] D hidden");
	printf ("\x1b[%i;0H", 6+cursorScreenPos);
	printf ("[ ] R system     [ ] L archive");
	printf ("\x1b[%i;0H", 7+cursorScreenPos);
	printf ("[ ]   virtual");
	printf ("\x1b[%i;1H", 7+cursorScreenPos);
	printf ((newAttribs & ATTR_VOLUME) ? "X" : " ");
	printf ("\x1b[%i;0H", 9+cursorScreenPos);
	printf ("(UDRL to change attributes)");
	while (1) {
		consoleSelect(&bottomConsole);
		printf ("\x1B[47m");		// Print foreground white color
		printf ("\x1b[%i;1H", 5+cursorScreenPos);
		printf ((newAttribs & ATTR_READONLY) ? "X" : " ");
		printf ("\x1b[%i;18H", 5+cursorScreenPos);
		printf ((newAttribs & ATTR_HIDDEN) ? "X" : " ");
		printf ("\x1b[%i;1H", 6+cursorScreenPos);
		printf ((newAttribs & ATTR_SYSTEM) ? "X" : " ");
		printf ("\x1b[%i;18H", 6+cursorScreenPos);
		printf ((newAttribs & ATTR_ARCHIVE) ? "X" : " ");
		printf ("\x1b[%i;0H", 11+cursorScreenPos);
		printf ((currentAttribs==newAttribs) ? "(<A> to continue)            " : "(<A> to apply, <B> to cancel)");

		consoleSelect(&topConsole);
		printf ("\x1B[30m");		// Print black color
		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf (" %s" ,RetTime().c_str());

			scanKeys();
			pressed = keysDown();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_RIGHT) && !(pressed & KEY_LEFT)
				&& !(pressed & KEY_A) && !(pressed & KEY_B));

		if (pressed & KEY_UP) {
			if (newAttribs & ATTR_READONLY) {
				newAttribs -= ATTR_READONLY;
			} else {
				newAttribs += ATTR_READONLY;
			}
		}

		if (pressed & KEY_DOWN) {
			if (newAttribs & ATTR_HIDDEN) {
				newAttribs -= ATTR_HIDDEN;
			} else {
				newAttribs += ATTR_HIDDEN;
			}
		}

		if (pressed & KEY_RIGHT) {
			if (newAttribs & ATTR_SYSTEM) {
				newAttribs -= ATTR_SYSTEM;
			} else {
				newAttribs += ATTR_SYSTEM;
			}
		}

		if (pressed & KEY_LEFT) {
			if (newAttribs & ATTR_ARCHIVE) {
				newAttribs -= ATTR_ARCHIVE;
			} else {
				newAttribs += ATTR_ARCHIVE;
			}
		}

		if ((pressed & KEY_A) && (currentAttribs!=newAttribs)) {
			FAT_setAttr(entry->name.c_str(), newAttribs);
			break;
		}

		if ((pressed & KEY_A) || (pressed & KEY_B)) {
			break;
		}
	}
}
