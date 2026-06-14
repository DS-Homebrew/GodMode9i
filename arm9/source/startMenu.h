#ifndef START_MENU
#define START_MENU

void startMenu(void);
// Returns true if the user confirmed a language selection (config already saved),
// false if they cancelled (caller should save the fallback).
// Pass cancellable=false to disable the B button (e.g. first-run forced selection).
bool languageMenu(bool cancellable = true);

#endif // START_MENU
