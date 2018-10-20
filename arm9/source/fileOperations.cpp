#include "fileOperations.h"
#include <nds.h>
#include <stdio.h>
#include <dirent.h>
#include <vector>

#include "file_browse.h"

using namespace std;

#define copyBufSize 0x8000

u32 copyBuf[copyBufSize];

char clipboard[256];
char clipboardFilename[256];
bool clipboardOn = false;
bool clipboardUsed = false;
bool clipboardDrive = false;	// false == SD card, true == Flashcard
bool clipboardInNitro = false;

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
			printf ("\x1b[16;0H");
			printf ("Progress:\n");
			printf ("%i/%i Bytes", (int)offset, (int)fsize);

			// Copy file to destination path
			numr = fread(copyBuf, 2, copyBufSize, sourceFile);
			fwrite(copyBuf, 2, numr, destinationFile);
			offset += copyBufSize;

			if (offset > fsize) {
				fclose(sourceFile);
				fclose(destinationFile);

				printf ("\x1b[17;0H");
				printf ("%i/%i Bytes", (int)fsize, (int)fsize);
				for (int i = 0; i < 60; i++) swiWaitForVBlank();

				return 1;
				break;
			}
		}

		return -1;
	}
}
