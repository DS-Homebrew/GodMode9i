// Language functions.
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include "font.h"

#include <string>

#define STRING(what, def) extern std::string STR_##what;
#include "language.inl"
#undef STRING

extern bool rtl;
extern int firstCol;
extern int lastCol;
extern Alignment alignStart;
extern Alignment alignEnd;

/**
 * Initialize translations.
 * Uses the language ID specified in settings.ui.language.
 *
 * Check the language variable outside of settings to determine
 * the actual language in use.
 */
void langInit(bool reloading);

#endif /* LANGUAGE_H */
