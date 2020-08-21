#include <nds.h>

#include "file_browse.h"

#ifndef FILE_COPY
#define FILE_COPY

struct ClipboardFile {
	std::string path;
	std::string name;
	bool folder;
	int drive; // 0 == SD card, 1 == Flashcard, 2 == RAMdrive 1, 3 == RAMdrive 2
	bool nitro;

	ClipboardFile(std::string path, std::string name, bool folder, int drive, bool nitro) : path(path), name(name), folder(folder), drive(drive), nitro(nitro) {}
};

extern std::vector<ClipboardFile> clipboard;
extern bool clipboardOn;
extern bool clipboardUsed;

extern void printBytes(int bytes);
extern void printBytesAlign(int bytes);

extern off_t getFileSize(const char *fileName);
extern int fcopy(const char *sourcePath, const char *destinationPath);
void changeFileAttribs(DirEntry* entry);

#endif // FILE_COPY
