#ifndef FILE_COPY
#define FILE_COPY

extern char clipboard[256];
extern char clipboardFilename[256];
extern bool clipboardOn;
extern bool clipboardUsed;
extern bool clipboardDrive;	// false == SD card, true == Flashcard

extern off_t getFileSize(const char *fileName);
extern int fcopy(const char *sourcePath, const char *destinationPath);

#endif // FILE_COPY