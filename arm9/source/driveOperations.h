#ifndef FLASHCARD_H
#define FLASHCARD_H

extern u8 stored_SCFG_MC;

extern bool sdMounted;
extern bool sdMountedDone;				// true if SD mount is successful once
extern bool flashcardMounted;
extern bool nitroMounted;

extern bool secondaryDrive;			// false == SD card, true == Flashcard
extern bool nitroSecondaryDrive;		// false == SD card, true == Flashcard

extern char sdLabel[12];
extern char fatLabel[12];

extern int sdSize;
extern int fatSize;

extern bool sdFound(void);
extern bool flashcardFound(void);
extern bool bothSDandFlashcard(void);
extern bool sdMount(void);
extern void sdUnmount(void);
extern bool flashcardMount(void);
extern void flashcardUnmount(void);

#endif //FLASHCARD_H
