#ifndef FLASHCARD_H
#define FLASHCARD_H

extern bool sdMounted;
extern bool flashcardMounted;

extern bool sdFound(void);
extern bool flashcardFound(void);
extern bool bothSDandFlashcard(void);
extern bool sdMount(void);
extern void sdUnmount(void);
extern bool flashcardMount(void);
extern void flashcardUnmount(void);

#endif //FLASHCARD_H
