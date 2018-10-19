#ifndef MAIN_H
#define MAIN_H

#define POWERTEXT_DS	"POWER - Poweroff"
#define POWERTEXT		"POWER - Reboot/[+held] Poweroff"
#define POWERTEXT_3DS	"POWER - Sleep Mode screen"
#define HOMETEXT 		"HOME - HOME Menu prompt"
#define SCREENSHOTTEXT 	"R+L - Make a screenshot"

extern char titleName[32];

extern int screenMode;

extern bool appInited;

extern bool arm7SCFGLocked;
extern bool isRegularDS;
extern bool is3DS;

extern bool applaunch;


#endif //MAIN_H
