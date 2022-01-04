#ifndef FLASHCARD_H
#define FLASHCARD_H

#include <string>
#include <nds/ndstypes.h>

enum class Drive : u8 {
	sdCard = 0,
	flashcard,
	ramDrive,
	nand,
	nitroFS,
	fatImg
};

extern bool nandMounted;
extern bool sdMounted;
extern bool sdMountedDone;				// true if SD mount is successful once
extern bool flashcardMounted;
extern bool ramdriveMounted;
extern bool imgMounted;
extern bool nitroMounted;

extern Drive currentDrive;
extern Drive nitroCurrentDrive;
extern Drive imgCurrentDrive;

extern char sdLabel[12];
extern char fatLabel[12];
extern char imgLabel[12];

extern u32 nandSize;
extern u64 sdSize;
extern u64 fatSize;
extern u64 imgSize;
extern u32 ramdSize;
extern std::string getDriveBytes(u64 bytes);

extern const char* getDrivePath(void);

extern bool nandFound(void);
extern bool sdFound(void);
extern bool flashcardFound(void);
extern bool bothSDandFlashcard(void);
extern bool imgFound(void);
extern bool nandMount(void);
extern void nandUnmount(void);
extern bool sdMount(void);
extern void sdUnmount(void);
extern bool flashcardMount(void);
extern void flashcardUnmount(void);
extern void ramdriveMount(bool ram32MB);
extern void nitroUnmount(void);
extern bool imgMount(const char* imgName);
extern void imgUnmount(void);
extern u64 getBytesFree(const char* drivePath);
extern bool driveWritable(Drive drive);
extern bool driveRemoved(Drive drive);

#endif //FLASHCARD_H
