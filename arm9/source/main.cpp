/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"
#include "driveMenu.h"
#include "driveOperations.h"
#include "file_browse.h"
#include "fileOperations.h"

#include "gm9i_logo.h"

char titleName[32] = {" "};

int screenMode = 0;

bool appInited = false;

bool arm7SCFGLocked = false;
bool isRegularDS = true;
bool is3DS = false;

bool applaunch = false;

static int bg3;

PrintConsole topConsole, bottomConsole;

using namespace std;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

bool extention(const std::string& filename, const char* ext) {
	if(strcasecmp(filename.c_str() + filename.size() - strlen(ext), ext)) {
		return false;
	} else {
		return true;
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	int pathLen;
	std::string filename;
	
	bool yHeld = false;

	sprintf(titleName, "GodMode9i v%i.%i.%i", 2, 1, 0);

	// initialize video mode
	videoSetMode(MODE_4_2D);

	// initialize VRAM banks
	vramSetPrimaryBanks(VRAM_A_MAIN_BG,
	                    VRAM_B_MAIN_SPRITE,
	                    VRAM_C_LCD,
	                    VRAM_D_LCD);

	// Subscreen as a console
	videoSetModeSub(MODE_0_2D);
	vramSetBankH(VRAM_H_SUB_BG);
	consoleInit(&bottomConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);

	// Display GM9i logo
	bg3 = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 1, 0);
	bgSetScroll(bg3, 0, 0);
	decompress(gm9i_logoBitmap, bgGetGfxPtr(bg3), LZ77Vram);

	printf ("\x1b[1;1H");
	printf(titleName);
	printf ("\x1b[2;1H");
	printf ("------------------------------");
	printf ("\x1b[3;1H");
	printf ("https:/github.com/");
	printf ("\x1b[4;11H");
	printf ("RocketRobz/GodMode9i");
	if (isDSiMode()) {
		printf ("\x1b[22;1H");
		printf ("Y Held - Disable cart access");
	}

	// Display for 2 seconds
	for (int i = 0; i < 60*2; i++) {
		swiWaitForVBlank();
	}

	fifoWaitValue32(FIFO_USER_06);
	if (fifoGetValue32(FIFO_USER_03) == 0) arm7SCFGLocked = true;
	u16 arm7_SNDEXCNT = fifoGetValue32(FIFO_USER_07);
	if (arm7_SNDEXCNT != 0) isRegularDS = false;	// If sound frequency setting is found, then the console is not a DS Phat/Lite
	fifoSendValue32(FIFO_USER_07, 0);

	if (isDSiMode()) {
		printf ("\x1b[22;1H");
		printf ("                            ");	// Clear "Y Held" text
	}
	printf ("\x1b[22;11H");
	printf ("mounting drive(s)...");

	sysSetCartOwner (BUS_OWNER_ARM9);	// Allow arm9 to access GBA ROM

	if (isDSiMode()) {
		scanKeys();
		if (keysHeld() & KEY_Y) {
			yHeld = true;
		}
		sdMounted = sdMount();
		*(vu32*)(0x0DFFFE0C) = 0x474D3969;		// Check for 32MB of RAM
		is3DS = ((access("sd:/Nintendo 3DS", F_OK) == 0) && (*(vu32*)(0x0DFFFE0C) == 0x474D3969));
	}
	if (!isDSiMode() || !yHeld) {
		flashcardMounted = flashcardMount();
		flashcardMountSkipped = false;
	}

	// Top screen as a console
	videoSetMode(MODE_0_2D);
	vramSetBankG(VRAM_G_MAIN_BG);
	consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

	keysSetRepeat(25,5);

	appInited = true;

	while(1) {

		if (screenMode == 0) {
			driveMenu();
		} else {
			filename = browseForFile();
		}

		if (applaunch) {
			// Construct a command line
			getcwd (filePath, PATH_MAX);
			pathLen = strlen (filePath);
			vector<char*> argarray;

			if ((strcasecmp (filename.c_str() + filename.size() - 5, ".argv") == 0)
			|| (strcasecmp (filename.c_str() + filename.size() - 5, ".ARGV") == 0)) {

				FILE *argfile = fopen(filename.c_str(),"rb");
				char str[PATH_MAX], *pstr;
				const char seps[]= "\n\r\t ";

				while( fgets(str, PATH_MAX, argfile) ) {
					// Find comment and end string there
					if( (pstr = strchr(str, '#')) )
						*pstr= '\0';

					// Tokenize arguments
					pstr= strtok(str, seps);

					while( pstr != NULL ) {
						argarray.push_back(strdup(pstr));
						pstr= strtok(NULL, seps);
					}
				}
				fclose(argfile);
				filename = argarray.at(0);
			} else {
				argarray.push_back(strdup(filename.c_str()));
			}

			if (extention(filename, ".dsi") || extention(filename, ".nds")) {
				char *name = argarray.at(0);
				strcpy (filePath + pathLen, name);
				free(argarray.at(0));
				argarray.at(0) = filePath;
				consoleClear();
				iprintf ("Running %s with %d parameters\n", argarray[0], argarray.size());
				int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0]);
				iprintf ("\x1b[31mStart failed. Error %i\n", err);
			}

			if (extention(filename, ".firm")) {
				char *name = argarray.at(0);
				strcpy (filePath + pathLen, name);
				free(argarray.at(0));
				argarray.at(0) = filePath;
				fcopy(argarray[0], "sd:/bootonce.firm");
				fifoSendValue32(FIFO_USER_02, 1);	// Reboot into selected .firm payload
				swiWaitForVBlank();
			}

			while(argarray.size() !=0 ) {
				free(argarray.at(0));
				argarray.erase(argarray.begin());
			}

			while (1) {
				swiWaitForVBlank();
				scanKeys();
				if (!(keysHeld() & KEY_A)) break;
			}
		}

	}

	return 0;
}
