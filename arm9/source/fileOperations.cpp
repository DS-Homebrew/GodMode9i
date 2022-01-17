#include "fileOperations.h"
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <dirent.h>
#include <vector>

#include "file_browse.h"
#include "font.h"
#include "ndsheaderbanner.h"
#include "screenshot.h"
#include "language.h"

#define copyBufSize 0x8000
#define shaChunkSize 0x10000

u32 copyBuf[copyBufSize];

std::vector<ClipboardFile> clipboard;
bool clipboardOn = false;
bool clipboardUsed = true;

static float getGbNumber(u64 bytes) {
	float gbNumber = 0.0f;
	for (u64 i = 0; i <= bytes; i += 0x6666666) {
		gbNumber += 0.1f;
	}
	return gbNumber;
}

static float getTbNumber(u64 bytes) {
	float tbNumber = 0.0f;
	for (u64 i = 0; i <= bytes; i += 0x1999999999) {
		tbNumber += 0.01f;
	}
	return tbNumber;
}

std::string getBytes(off_t bytes) {
	char buffer[32];
	if (bytes == 1)
		sniprintf(buffer, sizeof(buffer), STR_1_BYTE.c_str());

	else if (bytes < 1024)
		sniprintf(buffer, sizeof(buffer), STR_N_BYTES.c_str(), bytes);

	else if (bytes < (1024 * 1024))
		sniprintf(buffer, sizeof(buffer), STR_N_KB.c_str(), bytes >> 10);

	else if (bytes < (1024 * 1024 * 1024))
		sniprintf(buffer, sizeof(buffer), STR_N_MB.c_str(), bytes >> 20);

	else if (bytes < 0x10000000000)
		snprintf(buffer, sizeof(buffer), STR_N_GB_FLOAT.c_str(), getGbNumber(bytes));

	else
		snprintf(buffer, sizeof(buffer), STR_N_TB_FLOAT.c_str(), getTbNumber(bytes));

	return buffer;
}

off_t getFileSize(const char *fileName) {
	struct stat st;
	stat(fileName, &st);
	return st.st_size;
}

bool calculateSHA1(const char *fileName, u8 *sha1) {
	off_t fsize = getFileSize(fileName);
	u8 *buf = (u8*) malloc(shaChunkSize);
	if (!buf) {
		font->clear(false);
		font->print(0, 0, false, STR_COULD_NOT_ALLOCATE_BUFFER);
		font->update(false);
		for(int i = 0; i < 60 * 2; i++)
			swiWaitForVBlank();
		return false;
	}
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		font->clear(false);
		font->print(0, 0, false, STR_COULD_NOT_OPEN_FILE_READING);
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
	font->printf(0, 0, false, Alignment::left, Palette::white, STR_CALCULATING_SHA1.c_str(), fileName);

	int nameHeight = font->calcHeight(fileName);
	font->print(0, nameHeight + 2, false, STR_START_CANCEL);

	font->print(0, nameHeight + 4, false, STR_PROGRESS);
	font->print(0, nameHeight + 5, false, "[");
	font->print(-1, nameHeight + 5, false, "]");

	while (true) {
		size_t ret = fread(buf, 1, shaChunkSize, fp);
		if (!ret) break;
		swiSHA1Update(&ctx, buf, ret);
		scanKeys();
		int keys = keysHeld();
		if (keys & KEY_START) {
			free(buf);
			fclose(fp);
			return false;
		}

		font->print((ftell(fp) / (fsize / (SCREEN_COLS - 2))) + 1, nameHeight + 5, false, "=");
		font->printf(0, nameHeight + 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES_PROCESSED.c_str(), ftell(fp), fsize);
		font->update(false);
	}
	swiSHA1Final(sha1, &ctx);
	free(buf);
	fclose(fp);
	return true;
}

int trimNds(const char *fileName) {
	FILE *file = fopen(fileName, "rb");
	if(file) {
		sNDSHeaderExt ndsCardHeader;

		fread(&ndsCardHeader, 1, sizeof(ndsCardHeader), file);

		fseek(file, 0, SEEK_END);
		u32 fileSize = ftell(file);

		fclose(file);

		u32 romSize = ((ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0))
						? ndsCardHeader.twlRomSize : ndsCardHeader.romSize + 0x88;

		if(fileSize == romSize) {
			font->clear(false);
			font->print(0, 0, false, STR_FILE_ALREADY_TRIMMED + "\n\n" + STR_A_OK);
			font->update(false);

			do {
				swiWaitForVBlank();
				scanKeys();
			} while(!(keysDown() & KEY_A));
		} else {
			font->clear(false);
			font->printf(0, 0, false, Alignment::left, Palette::white, (STR_TRIM_TO_N_BYTES + "\n\n" + STR_A_YES_B_NO).c_str(), getBytes(romSize).c_str());
			font->update(false);

			u16 pressed;
			do {
				scanKeys();
				pressed = keysDown();
				swiWaitForVBlank();
			} while(!(pressed & (KEY_A | KEY_B)));

			if(pressed & KEY_A) {
				truncate(fileName, romSize);
				fileSize = romSize;
			}
		}

		return fileSize;
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
		font->print(0, 0, false, STR_PROGRESS);
		font->print(0, 1, false, "[");
		font->print(-1, 1, false, "]");

		off_t offset = 0;
		size_t numr;
		while (1) {
			scanKeys();
			if (keysHeld() & KEY_B) {
				// Cancel copying
				fclose(sourceFile);
				fclose(destinationFile);
				return -1;
				break;
			}

			font->print((offset / (fsize / (SCREEN_COLS - 2))) + 1, 1, false, "=");
			font->printf(0, 2, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), (int)offset, (int)fsize);
			font->update(false);

			// Copy file to destination path
			numr = fread(copyBuf, 1, copyBufSize, sourceFile);
			if(fwrite(copyBuf, 1, numr, destinationFile) != numr) {
				fclose(sourceFile);
				fclose(destinationFile);
				return -1;
			}
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
	struct stat st;
	if(!entry->isDirectory)
		stat(entry->name.c_str(), &st);

	while (1) {
		font->clear(false);
		font->print(0, 0, false, entry->name);
		if (!entry->isDirectory) {
			font->printf(0, cursorScreenPos + 1, false, Alignment::left, Palette::white, STR_FILESIZE.c_str(), getBytes(entry->size).c_str());

			char str[32];
			strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_ctime));
			font->printf(0, cursorScreenPos + 2, false, Alignment::left, Palette::white, STR_CREATED.c_str(), str);
			strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
			font->printf(0, cursorScreenPos + 3, false, Alignment::left, Palette::white, STR_MODIFIED.c_str(), str);
		}
		font->printf(0, cursorScreenPos + 5, false, Alignment::left, Palette::white, "[%c]%-13s[%c]%s", (newAttribs & ATTR_READONLY) ? 'X' : ' ', STR_UP_READONLY.c_str(), (newAttribs & ATTR_HIDDEN) ? 'X' : ' ', STR_LEFT_HIDDEN.c_str());
		font->printf(0, cursorScreenPos + 6, false, Alignment::left, Palette::white, "[%c]%-13s[%c]%s", (newAttribs & ATTR_SYSTEM) ? 'X' : ' ', STR_DOWN_SYSTEM.c_str(), (newAttribs & ATTR_ARCHIVE) ? 'X' : ' ', STR_RIGHT_ARCHIVE.c_str());
		font->printf(0, cursorScreenPos + 7, false, Alignment::left, Palette::white, "[%c] %s", (newAttribs & ATTR_VOLUME) ? 'X' : ' ', STR_VIRTUAL.c_str());
		font->printf(0, cursorScreenPos + 8, false, Alignment::left, Palette::white, STR_UDLR_CHANGE_ATTRIBUTES.c_str());
		font->print(0, cursorScreenPos + 10, false, (currentAttribs == newAttribs) ? STR_A_CONTINUE : STR_A_APPLY_B_CANCEL);
		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			held = keysHeld();
			pressed = keysDown();
			swiWaitForVBlank();
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_RIGHT) && !(pressed & KEY_LEFT)
				&& !(pressed & KEY_A) && !(pressed & KEY_B));

		if (pressed & KEY_UP) {
			newAttribs ^= ATTR_READONLY;
		} else if (pressed & KEY_DOWN) {
			newAttribs ^= ATTR_SYSTEM;
		} else if (pressed & KEY_RIGHT) {
			newAttribs ^= ATTR_ARCHIVE;
		} else if (pressed & KEY_LEFT) {
			newAttribs ^= ATTR_HIDDEN;
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
