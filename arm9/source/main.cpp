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
#include "config.h"
#include "driveMenu.h"
#include "driveOperations.h"
#include "file_browse.h"
#include "fileOperations.h"
#include "font.h"
#include "language.h"
#include "nitrofs.h"
#include "tonccpy.h"
#include "version.h"

#include "gm9i_logo.h"

char titleName[32] = {" "};

int screenMode = 0;

bool appInited = false;
#ifdef SCREENSWAP
bool screenSwapped = false;
#endif

bool arm7SCFGLocked = false;
bool isRegularDS = true;
bool expansionPakFound = false;
bool is3DS = false;

bool applaunch = false;

static int bg3;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

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

	sprintf(titleName, "GodMode9i %s", VER_NUMBER);

	// initialize video mode
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);

	// initialize VRAM banks
	vramSetPrimaryBanks(VRAM_A_MAIN_BG,
	                    VRAM_B_MAIN_SPRITE,
	                    VRAM_C_SUB_BG,
	                    VRAM_D_LCD);
	vramSetBankI(VRAM_I_SUB_SPRITE);

	// Init built-in font
	font = new Font(nullptr);

	// Display GM9i logo
	bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
	bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
	decompress(gm9i_logoBitmap, bgGetGfxPtr(bg3), LZ77Vram);
	tonccpy(BG_PALETTE, gm9i_logoPal, gm9i_logoPalLen);

	font->print(1, 1, false, titleName);
	font->print(1, 2, false, "---------------------------------------");
	font->print(1, 3, false, "https:/github.com/DS-Homebrew/GodMode9i");

	fifoWaitValue32(FIFO_USER_06);
	if (fifoGetValue32(FIFO_USER_03) == 0) arm7SCFGLocked = true;
	u16 arm7_SNDEXCNT = fifoGetValue32(FIFO_USER_07);
	if (arm7_SNDEXCNT != 0) isRegularDS = false;	// If sound frequency setting is found, then the console is not a DS Phat/Lite
	fifoSendValue32(FIFO_USER_07, 0);

	if (isDSiMode()) {
		if (!arm7SCFGLocked) {
			font->print(-2, -4, false, " Held - Disable NAND access", Alignment::right);
			font->print(-2, -3, false, " Held - Disable cart access", Alignment::right);
			font->print(-2, -2, false, "Do these if it crashes here", Alignment::right);
		} else {
			font->print(-2, -3, false, " Held - Disable NAND access", Alignment::right);
			font->print(-2, -2, false, "Do this if it crashes here", Alignment::right);
		}
	}

	// Display for 2 seconds
	font->update(false);
	for (int i = 0; i < 60*2; i++) {
		swiWaitForVBlank();
	}

	font->clear(false);
	font->print(1, 1, false, titleName);
	font->print(1, 2, false, "---------------------------------------");
	font->print(1, 3, false, "https:/github.com/DS-Homebrew/GodMode9i");
	font->print(-2, -2, false, "Mounting drive(s)...", Alignment::right);
	font->update(false);

	sysSetCartOwner (BUS_OWNER_ARM9);	// Allow arm9 to access GBA ROM

	if (isDSiMode() || !isRegularDS) {
		if (*(u8*)(0x2FFFD08) == 0) {
			sdMounted = sdMount();
		}
	}
	if (isDSiMode()) {
		scanKeys();
		yHeld = (keysHeld() & KEY_Y);
		ramdrive1Mount();
		*(vu32*)(0x0DFFFE0C) = 0x474D3969;		// Check for 32MB of RAM
		if (*(vu32*)(0x0DFFFE0C) == 0x474D3969) {
			ramdrive2Mount();
			is3DS = fifoGetValue32(FIFO_USER_05) != 0xD2;
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

	// Try to init NitroFS
	if (argc > 0 && nitroFSInit(argv[0]));
	else if (nitroFSInit("GodMode9i.nds"));
	else if (nitroFSInit("GodMode9i.dsi"));
	else if (nitroFSInit("sd:/GodMode9i.nds"));
	else if (nitroFSInit("sd:/GodMode9i.dsi"));
	else if (nitroFSInit("fat:/GodMode9i.nds"));
	else if (nitroFSInit("fat:/GodMode9i.dsi"));
	else {
		font->print(-2, -3, false, "NitroFS init failed...", Alignment::right);
		font->update(false);
		for (int i = 0; i < 30; i++)
			swiWaitForVBlank();
	}

	// Load config
	config = new Config();

	bgHide(bg3);

	// Reinit font, try to load default from SD this time
	delete font;
	font = new Font(config->fontPath().c_str());

	// Load translations
	langInit(false);

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
			std::vector<char*> argarray;

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
				filename = argarray[0];
			} else {
				argarray.push_back(strdup(filename.c_str()));
			}

			if (extension(filename, {"nds", "dsi", "ids", "app", "srl"})) {
				char *name = argarray[0];
				strcpy (filePath + pathLen, name);
				free(argarray[0]);
				argarray[0] = filePath;
				font->clear(false);
				font->printf(0, 0, false, Alignment::left, Palette::white, STR_RUNNING_X_WITH_N_PARAMETERS.c_str(), argarray[0], argarray.size());
				int err = runNdsFile(argarray[0], argarray.size(), (const char **)&argarray[0]);
				font->printf(0, 1, false, Alignment::left, Palette::white, STR_START_FAILED_ERROR_N.c_str(), err);
			}

			if (extension(filename, {"firm"})) {
				char *name = argarray[0];
				strcpy (filePath + pathLen, name);
				free(argarray[0]);
				argarray[0] = filePath;
				fcopy(argarray[0], "sd:/bootonce.firm");
				fifoSendValue32(FIFO_USER_02, 1);	// Reboot into selected .firm payload
				swiWaitForVBlank();
			}

			while(argarray.size() !=0 ) {
				free(argarray[0]);
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
