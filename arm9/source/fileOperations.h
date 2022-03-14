#include <nds.h>

#include "driveOperations.h"
#include "file_browse.h"

#ifndef FILE_COPY
#define FILE_COPY

struct ClipboardFile {
	std::string path;
	std::string name;
	bool folder;
	Drive drive;

	ClipboardFile(std::string path, std::string name, bool folder, Drive drive) : path(std::move(path)), name(std::move(name)), folder(folder), drive(drive) {}
};

extern std::vector<ClipboardFile> clipboard;
extern bool clipboardOn;
extern bool clipboardUsed;

extern std::string getBytes(off_t bytes);

extern off_t getFileSize(const char *fileName);
extern bool calculateSHA1(const char *fileName, u8 *sha1);
extern int trimNds(const char *fileName);
extern bool fcopy(const char *sourcePath, const char *destinationPath);
void changeFileAttribs(const DirEntry *entry);

#endif // FILE_COPY
