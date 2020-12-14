#ifndef FLASHCARD_H
#define FLASHCARD_H

extern u8 stored_SCFG_MC;

extern bool nandMounted;
extern bool sdMounted;
extern bool sdMountedDone;				// true if SD mount is successful once
extern bool flashcardMounted;
extern bool ramdrive1Mounted;
extern bool ramdrive2Mounted;
extern bool imgMounted;
extern bool nitroMounted;

extern int currentDrive;				// 0 == SD card, 1 == Flashcard, 2 == RAMdrive 1, 3 == RAMdrive 2, 4 == NAND, 5 == NitroFS, 6 == FAT IMG
extern int nitroCurrentDrive;
extern int imgCurrentDrive;

extern char sdLabel[12];
extern char fatLabel[12];
extern char imgLabel[12];

extern u32 nandSize;
extern u64 sdSize;
extern u64 fatSize;
extern u64 imgSize;
extern void printDriveBytes(u64 bytes);

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
extern void ramdrive1Mount(void);
extern void ramdrive2Mount(void);
extern void nitroUnmount(void);
extern bool imgMount(const char* imgName);
extern void imgUnmount(void);
extern u64 getBytesFree(const char* drivePath);

#endif //FLASHCARD_H
