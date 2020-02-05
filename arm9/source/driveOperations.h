#ifndef FLASHCARD_H
#define FLASHCARD_H

extern u8 stored_SCFG_MC;

extern bool sdMounted;
extern bool sdMountedDone;				// true if SD mount is successful once
extern bool flashcardMounted;
extern bool ramdrive1Mounted;
extern bool ramdrive2Mounted;
extern bool nitroMounted;

extern int currentDrive;				// 0 == SD card, 1 == Flashcard, 2 == RAMdrive 1, 3 == RAMdrive 2
extern int nitroCurrentDrive;

extern char sdLabel[12];
extern char fatLabel[12];

extern u64 sdSize;
extern u64 fatSize;
extern void printDriveBytes(u64 bytes);

extern const char* getDrivePath(void);

extern bool sdFound(void);
extern bool flashcardFound(void);
extern bool bothSDandFlashcard(void);
extern bool sdMount(void);
extern void sdUnmount(void);
extern bool flashcardMount(void);
extern void flashcardUnmount(void);
extern void ramdrive1Mount(void);
extern void ramdrive2Mount(void);

#endif //FLASHCARD_H
