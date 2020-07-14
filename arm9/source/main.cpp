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
#include <nds/arm9/dldi.h>
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
bool expansionPakFound = false;
bool is3DS = false;

bool applaunch = false;

static int bg3;

PrintConsole topConsoleBG, topConsole, bottomConsoleBG, bottomConsole;

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

void printBorderTop(void) {
	consoleSelect(&topConsoleBG);
	printf ("\x1B[42m");		// Print green color
	for (int i = 0; i < 32; i++) {
		printf ("\x02");	// Print top border
	}
}

void printBorderBottom(void) {
	consoleSelect(&bottomConsoleBG);
	printf ("\x1B[42m");		// Print green color
	for (int i = 0; i < 32; i++) {
		printf ("\x02");	// Print top border
	}
}

void clearBorderTop(void) {
	consoleSelect(&topConsoleBG);
	consoleClear();
}

void clearBorderBottom(void) {
	consoleSelect(&bottomConsoleBG);
	consoleClear();
}

void reinitConsoles(void) {
	// Subscreen as a console
	videoSetModeSub(MODE_0_2D);
	vramSetBankH(VRAM_H_SUB_BG);
	consoleInit(&bottomConsoleBG, 1, BgType_Text4bpp, BgSize_T_256x256, 7, 0, false, true);
	consoleInit(&bottomConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);

	// Top screen as a console
	videoSetMode(MODE_0_2D);
	vramSetBankG(VRAM_G_MAIN_BG);
	consoleInit(&topConsoleBG, 1, BgType_Text4bpp, BgSize_T_256x256, 7, 0, true, true);
	consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

	// Overwrite background white color
	BG_PALETTE[15+(7*16)] = 0x656A;
	BG_PALETTE_SUB[15+(7*16)] = 0x656A;

	// Overwrite 2nd smiley face with filled tile
	dmaFillWords(0xFFFFFFFF, (void*)0x6000040, 8*8);	// Top screen
	dmaFillWords(0xFFFFFFFF, (void*)0x6200040, 8*8);	// Bottom screen

	printBorderTop();
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

	sprintf(titleName, "GodMode9i v%i.%i.%i", 2, 3, 1);

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
	consoleInit(&bottomConsoleBG, 1, BgType_Text4bpp, BgSize_T_256x256, 7, 0, false, true);
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
		printf ("\x1b[20;1H");
		printf ("X Held - Disable NAND access");
		printf ("\x1b[21;1H");
		printf ("Y Held - Disable cart access");
		printf ("\x1b[22;4H");
		printf ("Do these if it crashes here");
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
		printf ("\x1b[21;1H");
		printf ("                            ");
		printf ("\x1b[22;5H");
		printf ("                          ");	// Clear "Y Held" text
	}
	printf ("\x1b[22;11H");
	printf ("mounting drive(s)...");
	//printf ("%X %X", *(u32*)0x2FFFD00, *(u32*)0x2FFFD04);

	sysSetCartOwner (BUS_OWNER_ARM9);	// Allow arm9 to access GBA ROM

	if (*(u8*)(0x2FFFD08) == 0) {
		sdMounted = sdMount();
	}
	if (isDSiMode()) {
		scanKeys();
		if (keysHeld() & KEY_Y) {
			yHeld = true;
		}
		ramdrive1Mount();
		*(vu32*)(0x0DFFFE0C) = 0x474D3969;		// Check for 32MB of RAM
		if (*(vu32*)(0x0DFFFE0C) == 0x474D3969) {
			ramdrive2Mount();
		}
		if (!(keysHeld() & KEY_X)) {
			nandMounted = nandMount();
		}
		//is3DS = ((access("sd:/Nintendo 3DS", F_OK) == 0) && (*(vu32*)(0x0DFFFE0C) == 0x474D3969));
		/*FILE* cidFile = fopen("sd:/gm9i/CID.bin", "wb");
		fwrite((void*)0x2FFD7BC, 1, 16, cidFile);
		fclose(cidFile);*/
		/*FILE* cidFile = fopen("sd:/gm9i/ConsoleID.bin", "wb");
		fwrite((void*)0x2FFFD00, 1, 8, cidFile);
		fclose(cidFile);*/
	} /*else if (isRegularDS) {
		*(vu32*)(0x08240000) = 1;
		expansionPakFound = ((*(vu32*)(0x08240000) == 1) && (io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS));
	}*/
	if (!isDSiMode() || !yHeld) {
		flashcardMounted = flashcardMount();
		flashcardMountSkipped = false;
	}

	// Top screen as a console
	videoSetMode(MODE_0_2D);
	vramSetBankG(VRAM_G_MAIN_BG);
	consoleInit(&topConsoleBG, 1, BgType_Text4bpp, BgSize_T_256x256, 7, 0, true, true);
	consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

	// Overwrite background white color
	BG_PALETTE[15+(7*16)] = 0x656A;
	BG_PALETTE_SUB[15+(7*16)] = 0x656A;

	// Overwrite 2nd smiley face with filled tile
	dmaFillWords(0xFFFFFFFF, (void*)0x6000040, 8*8);	// Top screen
	dmaFillWords(0xFFFFFFFF, (void*)0x6200040, 8*8);	// Bottom screen

	printBorderTop();

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

			if (extention(filename, ".nds") || extention(filename, ".dsi")
			 || extention(filename, ".ids") || extention(filename, ".app")) {
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
