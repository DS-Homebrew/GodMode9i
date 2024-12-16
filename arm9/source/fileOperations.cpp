#include "fileOperations.h"
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>

#include "sha1.h"
#include "file_browse.h"
#include "font.h"
#include "ndsheaderbanner.h"
#include "screenshot.h"
#include "language.h"

#define copyBufSize 0x8000
#define shaChunkSize 0x10000

// u8* copyBuf = (u8*)0x02004000;
u8 copyBuf[copyBufSize];

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
		font->print(firstCol, 0, false, STR_COULD_NOT_ALLOCATE_BUFFER, alignStart);
		font->update(false);
		for(int i = 0; i < 60 * 2; i++)
			swiWaitForVBlank();
		return false;
	}
	FILE* fp = fopen(fileName, "rb");
	if (!fp) {
		font->clear(false);
		font->print(firstCol, 0, false, STR_COULD_NOT_OPEN_FILE_READING, alignStart);
		font->update(false);
		for(int i = 0; i < 60 * 2; i++)
			swiWaitForVBlank();
		free(buf);
		return false;
	}
	memset(sha1, 0, 20);
	SHA1_CTX ctx;
	SHA1Init(&ctx);

	font->clear(false);
	font->printf(firstCol, 0, false, alignStart, Palette::white, STR_CALCULATING_SHA1.c_str(), fileName);

	int nameHeight = font->calcHeight(fileName);
	font->print(firstCol, nameHeight + 2, false, STR_START_CANCEL, alignStart);

	font->print(firstCol, nameHeight + 4, false, STR_PROGRESS, alignStart);
	font->print(0, nameHeight + 5, false, "[");
	font->print(-1, nameHeight + 5, false, "]");

	while (pmMainLoop()) {
		size_t ret = fread(buf, 1, shaChunkSize, fp);
		if (!ret) break;
		SHA1Update(&ctx, buf, ret);
		scanKeys();
		int keys = keysHeld();
		if (keys & KEY_START) {
			free(buf);
			fclose(fp);
			return false;
		}

		int progressPos = (ftell(fp) / (fsize / (SCREEN_COLS - 2))) + 1;
		if(rtl)
			progressPos = (progressPos + 1) * -1;
		font->print(progressPos, nameHeight + 5, false, "=");
		font->printf(firstCol, nameHeight + 6, false, alignStart, Palette::white, STR_N_OF_N_BYTES_PROCESSED.c_str(), ftell(fp), fsize);
		font->update(false);
	}
	SHA1Final(sha1, &ctx);
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
		fseek(file, 0, SEEK_SET);

		u32 romSize;
		if((ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0)) {
			romSize = ndsCardHeader.twlRomSize;
		} else {
			romSize = ndsCardHeader.romSize;

			// Check if it has an RSA key or not
			fseek(file, romSize, SEEK_SET);
			u16 magic;
			if(fread(&magic, sizeof(u16), 1, file) == 1) {
				// 'ac', auth code -- magic number
				if(magic == 0x6361)
					romSize += 0x88;
			}

		}

		fclose(file);

		if(fileSize == romSize) {
			font->clear(false);
			font->print(firstCol, 0, false, STR_FILE_ALREADY_TRIMMED + "\n\n" + STR_A_OK, alignStart);
			font->update(false);

			do {
				swiWaitForVBlank();
				scanKeys();
			} while(pmMainLoop() && !(keysDown() & KEY_A));
		} else {
			font->clear(false);
			font->printf(firstCol, 0, false, alignStart, Palette::white, (STR_TRIM_TO_N_BYTES + "\n\n" + STR_A_YES_B_NO).c_str(), getBytes(romSize).c_str());
			font->update(false);

			u16 pressed;
			do {
				scanKeys();
				pressed = keysDown();
				swiWaitForVBlank();
			} while(pmMainLoop() && !(pressed & (KEY_A | KEY_B)));

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

u64 dirSize(const std::vector<DirEntry> &dirContents) {
	u64 size = 0;

	for(const DirEntry &entry : dirContents) {
		if(entry.name == "." || entry.name == "..")
			continue;

		if(entry.isDirectory) {
			std::vector<DirEntry> subdirContents;
			if(chdir(entry.name.c_str()) == 0 && getDirectoryContents(subdirContents)) {
				size += dirSize(subdirContents);
				chdir("..");
			}
		} else {
			size += getFileSize(entry.name.c_str());
		}
	}

	return size;
}

bool fcopy(const char *sourcePath, const char *destinationPath) {
	DIR *isDir = opendir(sourcePath);
	
	if (isDir != NULL) {
		closedir(isDir);

		// Source path is a directory
		char startPath[PATH_MAX];
		getcwd(startPath, PATH_MAX);

		chdir(sourcePath);
		std::vector<DirEntry> dirContents;
		getDirectoryContents(dirContents);

		// Check that everything will fit
		if(dirSize(dirContents) > driveSizeFree(getDriveFromPath(destinationPath))) {
			font->clear(false);
			font->printf(0, 0, false, Alignment::left, Palette::white, (STR_FILE_TOO_BIG + "\n\n" + STR_A_OK).c_str(), sourcePath);
			font->update(false);

			do {
				swiWaitForVBlank();
				scanKeys();
			} while(pmMainLoop() && !(keysDown() & KEY_A));

			chdir(startPath);
			return false;
		}

		mkdir(destinationPath, 0777);
		for (int i = 1; i < ((int)dirContents.size()); i++) {
			chdir(sourcePath);
			dirCopy(dirContents[i], i, destinationPath, sourcePath);
		}

		chdir(destinationPath);
		chdir("..");
		return true;
	} else {
		closedir(isDir);

		// Source path is a file
		FILE* sourceFile = fopen(sourcePath, "rb");
		long fsize = 0;
		if (sourceFile) {
			fseek(sourceFile, 0, SEEK_END);
			fsize = ftell(sourceFile); // Get source file's size
			fseek(sourceFile, 0, SEEK_SET);
		} else {
			return false;
		}

		// Check that the file will fit
		if((u64)fsize > driveSizeFree(getDriveFromPath(destinationPath))) {
			font->clear(false);
			font->printf(0, 0, false, Alignment::left, Palette::white, (STR_FILE_TOO_BIG + "\n\n" + STR_A_OK).c_str(), sourcePath);
			font->update(false);

			do {
				swiWaitForVBlank();
				scanKeys();
			} while(pmMainLoop() && !(keysDown() & KEY_A));

			return false;
		}

		FILE* destinationFile = fopen(destinationPath, "wb");
		if (!destinationFile) {
			fclose(sourceFile);
			return false;
		}

		font->clear(false);
		font->print(firstCol, 0, false, STR_PROGRESS, alignStart);
		font->print(0, 1, false, "[");
		font->print(-1, 1, false, "]");

		off_t offset = 0;
		size_t numr;
		while (pmMainLoop()) {
			scanKeys();
			if (keysHeld() & KEY_B) {
				// Cancel copying
				fclose(sourceFile);
				fclose(destinationFile);
				return false;
			}

			int progressPos = (offset / (fsize / (SCREEN_COLS - 2))) + 1;
			if(rtl)
				progressPos = (progressPos + 1) * -1;
			font->print(progressPos, 1, false, "=");
			font->printf(firstCol, 2, false, alignStart, Palette::white, STR_N_OF_N_BYTES.c_str(), (int)offset, (int)fsize);
			font->update(false);

			// Copy file to destination path
			numr = fread(copyBuf, 1, copyBufSize, sourceFile);
			if(fwrite(copyBuf, 1, numr, destinationFile) != numr) {
				fclose(sourceFile);
				fclose(destinationFile);
				return false;
			}
			offset += copyBufSize;

			if (offset > fsize) {
				fclose(sourceFile);
				fclose(destinationFile);

				return true;
				break;
			}
		}

		return false;
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

	while (pmMainLoop()) {
		font->clear(false);
		font->print(firstCol, 0, false, entry->name, alignStart);
		if (!entry->isDirectory) {
			font->printf(firstCol, cursorScreenPos + 1, false, alignStart, Palette::white, STR_FILESIZE.c_str(), getBytes(entry->size).c_str());

			char str[32];
			strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_ctime));
			font->printf(firstCol, cursorScreenPos + 2, false, alignStart, Palette::white, STR_CREATED.c_str(), str);
			strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
			font->printf(firstCol, cursorScreenPos + 3, false, alignStart, Palette::white, STR_MODIFIED.c_str(), str);
		}
		font->printf(firstCol, cursorScreenPos + 5, false, alignStart, Palette::white, "[%c]%-13s[%c]%s", (newAttribs & ATTR_READONLY) ? 'X' : ' ', STR_UP_READONLY.c_str(), (newAttribs & ATTR_HIDDEN) ? 'X' : ' ', STR_LEFT_HIDDEN.c_str());
		font->printf(firstCol, cursorScreenPos + 6, false, alignStart, Palette::white, "[%c]%-13s[%c]%s", (newAttribs & ATTR_SYSTEM) ? 'X' : ' ', STR_DOWN_SYSTEM.c_str(), (newAttribs & ATTR_ARCHIVE) ? 'X' : ' ', STR_RIGHT_ARCHIVE.c_str());
		font->printf(firstCol, cursorScreenPos + 7, false, alignStart, Palette::white, "[%c] %s", (newAttribs & ATTR_VOLUME) ? 'X' : ' ', STR_VIRTUAL.c_str());
		if(driveWritable(currentDrive))
			font->printf(firstCol, cursorScreenPos + 8, false, alignStart, Palette::white, STR_UDLR_CHANGE_ATTRIBUTES.c_str());
		font->print(firstCol, cursorScreenPos + 10, false, (currentAttribs == newAttribs) ? STR_A_CONTINUE : STR_A_APPLY_B_CANCEL, alignStart);
		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			held = keysHeld();
			pressed = keysDown();
			swiWaitForVBlank();
		} while (pmMainLoop() && !(pressed & (KEY_UP | KEY_DOWN | KEY_RIGHT | KEY_LEFT | KEY_A | KEY_B)));

		if(driveWritable(currentDrive)) {
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
			}
		}

		if (pressed & (KEY_A | KEY_B)) {
			break;
		} else if (held & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}
