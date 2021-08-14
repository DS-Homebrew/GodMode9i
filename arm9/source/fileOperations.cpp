#include "fileOperations.h"
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <dirent.h>
#include <vector>

#include "date.h"
#include "file_browse.h"
#include "font.h"
#include "ndsheaderbanner.h"
#include "screenshot.h"

#define copyBufSize 0x8000
#define shaChunkSize 0x10000

u32 copyBuf[copyBufSize];

std::vector<ClipboardFile> clipboard;
bool clipboardOn = false;
bool clipboardUsed = false;

std::string getBytes(int bytes) {
	char buffer[11];
	if (bytes == 1)
		sniprintf(buffer, sizeof(buffer), "%d Byte", bytes);

	else if (bytes < 1024)
		sniprintf(buffer, sizeof(buffer), "%d Bytes", bytes);

	else if (bytes < (1024 * 1024))
		sniprintf(buffer, sizeof(buffer), "%d KB", bytes / 1024);

	else if (bytes < (1024 * 1024 * 1024))
		sniprintf(buffer, sizeof(buffer), "%d MB", bytes / 1024 / 1024);

	else
		sniprintf(buffer, sizeof(buffer), "%d GB", bytes / 1024 / 1024 / 1024);

	return buffer;
}

off_t getFileSize(const char *fileName) {
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

bool calculateSHA1(const char *fileName, u8 *sha1) {
	off_t fsize = getFileSize(fileName);
	u8 *buf = (u8*) malloc(shaChunkSize);
	if (!buf) {
		font->clear(false);
		font->print(0, 0, false, "Could not allocate buffer");
		font->update(false);
		for(int i = 0; i < 60 * 2; i++)
			swiWaitForVBlank();
		return false;
	}
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		font->clear(false);
		font->print(0, 0, false, "Could not open file for reading");
		font->update(false);
		for(int i = 0; i < 60 * 2; i++)
			swiWaitForVBlank();
		free(buf);
		return false;
	}
	memset(sha1, 0, 20);
	swiSHA1context_t ctx;
	ctx.sha_block=0; //this is weird but it has to be done
	swiSHA1Init(&ctx);

	font->clear(false);
	font->print(0, 0, false, "Calculating SHA1 hash of:");
	font->print(0, 1, false, fileName);

	int nameHeight = font->calcHeight(fileName);
	font->print(0, nameHeight + 2, false, "(<START> to cancel)");

	font->print(0, nameHeight + 4, false, "Progress:");
	font->print(0, nameHeight + 5, false, "[");
	font->print(-1, nameHeight + 5, false, "]");

	while (true) {
		size_t ret = fread(buf, 1, shaChunkSize, fp);
		if (!ret) break;
		swiSHA1Update(&ctx, buf, ret);
		scanKeys();
		int keys = keysHeld();
		if (keys & KEY_START) return false;

		font->print((ftell(fp) / (fsize / (SCREEN_COLS - 2))) + 1, nameHeight + 5, false, "=");
		font->printf(0, nameHeight + 6, false, Alignment::left, Palette::white, "%d/%d bytes processed", ftell(fp), fsize);
		font->update(false);
	}
	swiSHA1Final(sha1, &ctx);
	free(buf);
	return true;
}

int trimNds(const char *fileName) {
	FILE *file = fopen(fileName, "rb");
	if(file) {
		sNDSHeaderExt ndsCardHeader;

		fread(&ndsCardHeader, 1, sizeof(ndsCardHeader), file);
		fclose(file);

		u32 romSize = ((ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0))
						? ndsCardHeader.twlRomSize : ndsCardHeader.romSize + 0x88;

		truncate(fileName, romSize);

		return romSize;
	}

	return -1;
}

void dirCopy(const DirEntry &entry, int i, const char *destinationPath, const char *sourcePath) {
	std::vector<DirEntry> dirContents;
	dirContents.clear();
	if (entry.isDirectory)	chdir((sourcePath + ("/" + entry.name)).c_str());
	getDirectoryContents(dirContents);
	if (((int)dirContents.size()) == 1)	mkdir((destinationPath + ("/" + entry.name)).c_str(), 0777);
	if (((int)dirContents.size()) != 1)	fcopy((sourcePath + ("/" + entry.name)).c_str(), (destinationPath + ("/" + entry.name)).c_str());
}

int fcopy(const char *sourcePath, const char *destinationPath) {
	DIR *isDir = opendir(sourcePath);
	
	if (isDir != NULL) {
		closedir(isDir);

		// Source path is a directory
		chdir(sourcePath);
		std::vector<DirEntry> dirContents;
		getDirectoryContents(dirContents);

		mkdir(destinationPath, 0777);
		for (int i = 1; i < ((int)dirContents.size()); i++) {
			chdir(sourcePath);
			dirCopy(dirContents[i], i, destinationPath, sourcePath);
		}

		chdir(destinationPath);
		chdir("..");
		return 1;
	} else {
		closedir(isDir);

		// Source path is a file
		FILE* sourceFile = fopen(sourcePath, "rb");
		off_t fsize = 0;
		if (sourceFile) {
			fseek(sourceFile, 0, SEEK_END);
			fsize = ftell(sourceFile); // Get source file's size
			fseek(sourceFile, 0, SEEK_SET);
		} else {
			return -1;
		}

		FILE* destinationFile = fopen(destinationPath, "wb");
		if (!destinationFile) {
			fclose(sourceFile);
			return -1;
		}

		font->clear(false);
		font->print(0, 0, false, "Progress:");
		font->print(0, 1, false, "[");
		font->print(-1, 1, false, "]");

		off_t offset = 0;
		int numr;
		while (1) {
			scanKeys();
			if (keysHeld() & KEY_B) {
				// Cancel copying
				fclose(sourceFile);
				fclose(destinationFile);
				return -1;
				break;
			}

			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);

			font->print((offset / (fsize / (SCREEN_COLS - 2))) + 1, 1, false, "=");
			font->printf(0, 2, false, Alignment::left, Palette::white, "%lld/%lld Bytes", offset, fsize);
			font->update(false);

			// Copy file to destination path
			numr = fread(copyBuf, 1, copyBufSize, sourceFile);
			fwrite(copyBuf, 1, numr, destinationFile);
			offset += copyBufSize;

			if (offset > fsize) {
				fclose(sourceFile);
				fclose(destinationFile);

				return 1;
				break;
			}
		}

		return -1;
	}
}

void changeFileAttribs(const DirEntry *entry) {
	int pressed = 0, held = 0;
	int cursorScreenPos = font->calcHeight(entry->name);
	uint8_t currentAttribs = FAT_getAttr(entry->name.c_str());
	uint8_t newAttribs = currentAttribs;

	while (1) {
		font->clear(false);
		font->print(0, 0, false, entry->name);
		if (!entry->isDirectory)
			font->printf(0, cursorScreenPos + 1, false, Alignment::left, Palette::white, "filesize: %s", getBytes(entry->size).c_str());
		font->printf(0, cursorScreenPos + 3, false, Alignment::left, Palette::white, "[%c] ↑ read-only  [%c] ↓ hidden", (newAttribs & ATTR_READONLY) ? 'X' : ' ', (newAttribs & ATTR_HIDDEN) ? 'X' : ' ');
		font->printf(0, cursorScreenPos + 4, false, Alignment::left, Palette::white, "[%c] → system     [%c] ← archive", (newAttribs & ATTR_SYSTEM) ? 'X' : ' ', (newAttribs & ATTR_ARCHIVE) ? 'X' : ' ');
		font->printf(0, cursorScreenPos + 5, false, Alignment::left, Palette::white, "[%c]   virtual", (newAttribs & ATTR_VOLUME) ? 'X' : ' ');
		font->printf(0, cursorScreenPos + 6, false, Alignment::left, Palette::white, "(↑↓→← to change attributes)");
		font->print(0, cursorScreenPos + 8, false, (currentAttribs == newAttribs) ? "(<A> to continue)" : "(<A> to apply, <B> to cancel)");
		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);

			scanKeys();
			held = keysHeld();
			pressed = keysDown();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_RIGHT) && !(pressed & KEY_LEFT)
				&& !(pressed & KEY_A) && !(pressed & KEY_B));

		if (pressed & KEY_UP) {
			newAttribs ^= ATTR_READONLY;
		} else if (pressed & KEY_DOWN) {
			newAttribs ^= ATTR_HIDDEN;
		} else if (pressed & KEY_RIGHT) {
			newAttribs ^= ATTR_SYSTEM;
		} else if (pressed & KEY_LEFT) {
			newAttribs ^= ATTR_ARCHIVE;
		} else if ((pressed & KEY_A) && (currentAttribs != newAttribs)) {
			FAT_setAttr(entry->name.c_str(), newAttribs);
			break;
		} else if (pressed & (KEY_A | KEY_B)) {
			break;
		} else if (held & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}
