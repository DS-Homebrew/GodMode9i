#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>
#include <string>

#include "nds_loader_arm9.h"
#include "config.h"
#include "date.h"
#include "diagnostics.h"
#include "driveMenu.h"
#include "driveOperations.h"
#include "file_browse.h"
#include "fileOperations.h"
#include "font.h"
#include "language.h"
#include "startMenu.h"
#include "my_sd.h"
#include "nandio.h"
#include "nitrofs.h"
#include "tonccpy.h"
#include "version.h"

#include "gm9i_logo.h"

char titleName[64] = {" "};

int screenMode = 0;

bool appInited = false;
bool screenSwapped = false;

bool arm7SCFGLocked = false;
bool isRegularDS = true;
bool isDSLite = false;
// bool bios9iEnabled = false;
bool is3DS = false;
int ownNitroFSMounted;
std::string prevTime;

bool applaunch = false;

static int bg3;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

//---------------------------------------------------------------------------------
void vblankHandler (void) {
//---------------------------------------------------------------------------------
	// Check if NDS cart ejected
	if(isDSiMode() && (REG_SCFG_MC & BIT(0)) && romTitle[0][0] != '\0') {
		romTitle[0][0] = '\0';
		romSizeTrimmed = romSize[0] = 0;
	}

	// Check if GBA cart ejected
	if(isRegularDS && (io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS) && *(u8*)(0x080000B2) != 0x96 && romTitle[1][0] != '\0') {
		romTitle[1][0] = '\0';
		romSize[1] = 0;
	}

	// Print time
	std::string time = RetTime();
	if(time != prevTime) {
		prevTime = time;
		if(font) {
			font->print(lastCol, 0, true, time, alignEnd, Palette::blackGreen);
			font->update(true);
		}
	}
}

//---------------------------------------------------------------------------------
// Describe the detected console model and the mode GodMode9i is running in.
// Requires isRegularDS/is3DS to be detected first (see the FIFO sync in main()).
static std::string getConsoleModeStr(void) {
//---------------------------------------------------------------------------------
	std::string model;
	if (is3DS)
		model = "Nintendo 3DS";
	else if (!isRegularDS)
		model = "Nintendo DSi";
	else if (isDSLite)
		model = "Nintendo DS Lite";
	else
		model = "Nintendo DS";
	return model + (isDSiMode() ? " (DSi Mode)" : " (DS Mode)");
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

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

	std::string bootSource = "Unknown";
	if (argc > 0 && argv[0]) {
		std::string path(argv[0]);
		if (path.compare(0, 4, "sd:/") == 0) {
			bootSource = "SD Card (sd:/)";
		} else if (path.compare(0, 5, "fat:/") == 0) {
			bootSource = "Flashcard (fat:/)";
		} else if (path.compare(0, 6, "nand:/") == 0) {
			bootSource = "NAND (nand:/)";
		} else {
			size_t colon = path.find(':');
			if (colon != std::string::npos) {
				bootSource = "Drive (" + path.substr(0, colon + 1) + ")";
			} else {
				bootSource = path;
			}
		}
	} else {
		if (isDSiMode()) {
			bootSource = (strcmp(io_dldi_data->friendlyName, "Default (No interface)") == 0) ? "NAND/SD (DSi Mode)" : "Flashcard (Slot-1)";
		} else {
			bootSource = "Flashcard (Slot-1)";
		}
	}

	font->print(1, 1, false, titleName);
	font->print(1, 2, false, "----------------------------------------");
	font->print(1, 3, false, "https://github.com/DS-Homebrew/GodMode9i");
	font->print(1, 5, false, "Booted from: " + bootSource);

	fifoWaitValue32(FIFO_USER_06);
	if (fifoGetValue32(FIFO_USER_03) == 0) arm7SCFGLocked = true;
	u32 arm7_07 = fifoGetValue32(FIFO_USER_07);
	u16 arm7_SNDEXCNT = arm7_07 & 0xFFFF;
	u8 arm7_wifiBoard = (arm7_07 >> 16) & 0xFF;
	u8 arm7_scfgOp = (arm7_07 >> 24) & 0xFF;	// SCFG_OP debugger type; 0xFF = not applicable (DS)
	u16 arm7_wifiChipId = fifoGetValue32(FIFO_USER_08) & 0xFFFF;	// WiFi chip ID (W_ID): 1440h=DS, C340h=DS Lite
	u32 arm7_macLo = fifoGetValue32(FIFO_USER_08);			// MAC bytes 0-3
	u32 arm7_macHi = fifoGetValue32(FIFO_USER_08) & 0xFFFF;		// MAC bytes 4-5
	u32 arm7_jedecId = fifoGetValue32(FIFO_USER_08) & 0xFFFFFF;	// firmware flash JEDEC ID
	if (arm7_SNDEXCNT != 0) isRegularDS = false;	// If sound frequency setting is found, then the console is not a DS Phat/Lite
	fifoSendValue32(FIFO_USER_07, 0);

	// DS Lite detection via the arm7's WiFi controller chip ID (W_ID): on a regular DS it reads
	// 1440h on the original DS and C340h on the DS Lite. That ID lives in WiFi silicon, not in a
	// reflashable firmware byte, so it can't be spoofed by cross-flashing. It also reads the WiFi
	// chip rather than the power-management chip, so the late "CPU-20" DS Phat (standard-Phat WiFi
	// but a DS-Lite PMIC) still reads 1440h and is correctly seen as a Phat, where power-chip checks
	// mislabel it. (A DSi/3DS also reports C340h, but those are already excluded by isRegularDS above.)
	if (isRegularDS && arm7_wifiChipId == 0xC340) isDSLite = true;

	// Detect RAM size and console model up front (3DS has 32MB of RAM).
	// arm7 sends FIFO_USER_05 before the sync above, so this is ready here.
	bool ram32MB = false;
	if (isDSiMode() || REG_SCFG_EXT != 0) {
		*(vu32*)(0x0DFFFE0C) = 0x474D3969;		// Check for 32MB of RAM
		ram32MB = *(vu32*)(0x0DFFFE0C) == 0x474D3969;
		if (ram32MB) {
			is3DS = fifoGetValue32(FIFO_USER_05) != 0xD2;
		}
	}

	font->print(1, 6, false, "Running on:  " + getConsoleModeStr());

	// Affordance for the hardware-info screen. Plain literal (not a localized STR_ string)
	// because this splash draws before langInit(), where STR_ strings would still be empty.
	font->print(1, 8, false, "Press SELECT for hardware info");

	HardwareInfo hwInfo = {};
	// Console type — GBATEK 01Dh spelling, derived from the reliable W_ID/is3DS detection above
	// (NOT the app's getConsoleModeStr(), which keeps its own string for the boot splash).
	if (is3DS)              hwInfo.consoleType = "Nintendo 3DS";
	else if (!isRegularDS)  hwInfo.consoleType = "Nintendo DSi";
	else if (isDSLite)      hwInfo.consoleType = "Nintendo DS-lite";
	else                    hwInfo.consoleType = "Nintendo DS";

	hwInfo.wifiChipId = arm7_wifiChipId;
	hwInfo.jedecId = arm7_jedecId;
	hwInfo.ramMB = is3DS ? 32 : (isRegularDS ? 4 : 16);
	hwInfo.mac[0] = arm7_macLo & 0xFF;
	hwInfo.mac[1] = (arm7_macLo >> 8) & 0xFF;
	hwInfo.mac[2] = (arm7_macLo >> 16) & 0xFF;
	hwInfo.mac[3] = (arm7_macLo >> 24) & 0xFF;
	hwInfo.mac[4] = arm7_macHi & 0xFF;
	hwInfo.mac[5] = (arm7_macHi >> 8) & 0xFF;

	// Unit (SCFG_OP): 0xFF sentinel means SCFG not present (a plain DS) → omit.
	hwInfo.hasUnit = (arm7_scfgOp != 0xFF);
	hwInfo.unitRetail = (arm7_scfgOp == 0);

	// WiFi board: firmware 1FDh is FFh on the original DS, so only 01/02/03 are shown.
	hwInfo.hasWifiBoard = (arm7_wifiBoard >= 1 && arm7_wifiBoard <= 3);
	hwInfo.wifiBoard = arm7_wifiBoard;

	// SCFG_EXT exists only on the DSi family.
	hwInfo.hasScfgExt = isDSiMode() || (REG_SCFG_EXT != 0);
	hwInfo.scfgExt = REG_SCFG_EXT;

	// Region, Serial and Console ID are DSi-only and need the NAND (not mounted yet here). They
	// are filled in later, right before the screen opens — see the diagnosticsShow call below.

	if (isDSiMode()) {
		// bios9iEnabled = true;
		if (!arm7SCFGLocked) {
			//font->print(-2, -4, false, " Held - Disable NAND access", Alignment::right);
			font->print(-2, -3, false, " Held - Disable cart access", Alignment::right);
			font->print(-2, -2, false, "Do this if it crashes here", Alignment::right);
		} /*else {
			font->print(-2, -3, false, " Held - Disable NAND access", Alignment::right);
			font->print(-2, -2, false, "Do this if it crashes here", Alignment::right);
		}*/
	}

	// Display for 2 seconds; latch a SELECT hold to open the hardware info screen later.
	bool diagnosticsRequested = false;
	font->update(false);
	for (int i = 0; i < 60*2; i++) {
		scanKeys();
		if (!diagnosticsRequested && (keysHeld() & KEY_SELECT)) {
			diagnosticsRequested = true;
			// Confirm the hold registered; the screen itself opens later, after drives mount.
			font->print(1, 8, false, "Hardware info selected, opening...");
			font->update(false);
		}
		swiWaitForVBlank();
	}

	font->clear(false);
	font->print(1, 1, false, titleName);
	font->print(1, 2, false, "----------------------------------------");
	font->print(1, 3, false, "https://github.com/DS-Homebrew/GodMode9i");
	font->print(1, 5, false, "Booted from: " + bootSource);
	font->print(1, 6, false, "Running on:  " + getConsoleModeStr());
	font->print(-2, -2, false, "Mounting drive(s)...", Alignment::right);
	font->update(false);

	sysSetCartOwner (BUS_OWNER_ARM9);	// Allow arm9 to access GBA ROM

	if (isDSiMode() || !isRegularDS) {
		fifoSetValue32Handler(FIFO_USER_04, sdStatusHandler, nullptr);
		if (!sdRemoved) {
			sdMounted = sdMount();
		}
	}
	bool nandMounted = false;
	if (isDSiMode()) {
		scanKeys();
		yHeld = (keysHeld() & KEY_Y);
		ramdriveMount(ram32MB);
		//if (!(keysHeld() & KEY_X)) {
			nandMounted = nandMount();
		//}
		//is3DS = ((access("sd:/Nintendo 3DS", F_OK) == 0) && (*(vu32*)(0x0DFFFE0C) == 0x474D3969));
		/*FILE* cidFile = fopen("sd:/gm9i/CID.bin", "wb");
		fwrite((void*)0x2FFD7BC, 1, 16, cidFile);
		fclose(cidFile);*/
		/*FILE* cidFile = fopen("sd:/gm9i/ConsoleID.bin", "wb");
		fwrite((void*)0x2FFFD00, 1, 8, cidFile);
		fclose(cidFile);*/
	} else if (REG_SCFG_EXT != 0) {
		ramdriveMount(ram32MB);

		/* FILE* bios = fopen("sd:/_nds/bios9i.bin", "rb");
		if (!bios) {
			bios = fopen("sd:/_nds/bios9i_part1.bin", "rb");
		}
		if (bios) {
			tonccpy((u32*)0x02008000, (u32*)0x02000000, 0x4000);

			extern u8* copyBuf;
			copyBuf = new u8[0x8000];

			fread((u32*)0x02000000, 1, 0x8000, bios);
			fclose(bios);

			// Relocate addresses
			*(u32*)0x020000CC -= 0xFFFF0000;
			*(u32*)0x02003264 -= 0xFFFF0000;
			*(u32*)0x02003268 -= 0xFFFF0000;
			*(u32*)0x0200326C -= 0xFFFF0000;
			*(u32*)0x020033E0 -= 0xFFFF0000;
			*(u32*)0x020042C0 -= 0xFFFF0000;
			*(u32*)0x02004B88 -= 0xFFFF0000;
			*(u32*)0x02004B90 -= 0xFFFF0000;
			*(u32*)0x02004B9C -= 0xFFFF0000;
			*(u32*)0x02004BA0 -= 0xFFFF0000;
			*(u32*)0x02004E1C -= 0xFFFF0000;
			*(u32*)0x02004F18 -= 0xFFFF0000;

			*(u32*)0x020000CC += 0x02000000;
			*(u32*)0x02003264 += 0x02000000;
			*(u32*)0x02003268 += 0x02000000;
			*(u32*)0x0200326C += 0x02000000;
			*(u32*)0x020033E0 += 0x02000000;
			*(u32*)0x020042C0 += 0x02000000;
			*(u32*)0x02004B88 += 0x02000000;
			*(u32*)0x02004B90 += 0x02000000;
			*(u32*)0x02004B9C += 0x02000000;
			*(u32*)0x02004BA0 += 0x02000000;
			*(u32*)0x02004E1C += 0x02000000;
			*(u32*)0x02004F18 += 0x02000000;

			u32* itcmAddr = (u32*)0x01000000;
			for (int i = 0; i < 8; i++) {
				itcmAddr[i] = 0xEA7FFFFE;
			}

			setVectorBase(0);
			bios9iEnabled = true; */

			nandMount();
		// }
	} else if (isRegularDS && (io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS)) {
		ramdriveMount(false);
	}
	if (!isDSiMode() || !yHeld) {
		flashcardMounted = flashcardMount();
		flashcardMountSkipped = false;
	}

	// Try to init NitroFS
	char nandPath[64] = {0};
	char sdnandPath[64] = {0};
	if (isDSiMode()) {
		if (nandMounted) {
			sprintf(nandPath, "nand:/title/%08x/%08x/content/000000%02x.app", *(unsigned int*)0x02FFE234, *(unsigned int*)0x02FFE230, *(u8*)0x02FFE01E);
			if (is3DS) {
				// The .app file is named differently on 3DS
				sprintf(nandPath, "nand:/title/%08x/%08x/content/00000000.tmd", *(unsigned int*)0x02FFE234, *(unsigned int*)0x02FFE230);
				FILE* tmdFile = fopen(nandPath, "rb");
				if (tmdFile) {
					u8 appNameTemp[4] = {0};
					u8 appName8[4] = {0};
					fseek(tmdFile, 0xB04, SEEK_SET);
					fread(appNameTemp, 1, 4, tmdFile);
					fclose(tmdFile);
					for (int i = 0; i < 4; i++) {
						appName8[i] = appNameTemp[3-i];
					}
					u32 appName = 0;
					tonccpy(&appName, appName8, 4);

					sprintf(nandPath, "nand:/title/%08x/%08x/content/%08x.app", *(unsigned int*)0x02FFE234, *(unsigned int*)0x02FFE230, (unsigned int)appName);
				}
			}
		}
		sprintf(sdnandPath, "sd:/title/%08x/%08x/content/000000%02x.app", *(unsigned int*)0x02FFE234, *(unsigned int*)0x02FFE230, *(u8*)0x02FFE01E);
	}
	ownNitroFSMounted = 0;
	nitroMounted = true;
	if (argc > 0 && nitroFSInit(argv[0])) nitroCurrentDrive = argv[0][0] == 's' ? Drive::sdCard : Drive::flashcard;
	else if (nandPath[0] && nitroFSInit(nandPath)) nitroCurrentDrive = Drive::nand;
	else if (sdnandPath[0] && nitroFSInit(sdnandPath)) nitroCurrentDrive = Drive::sdCard;
	else if (nitroFSInit("sd:/GodMode9i.nds")) nitroCurrentDrive = Drive::sdCard;
	else if (nitroFSInit("sd:/GodMode9i.dsi")) nitroCurrentDrive = Drive::sdCard;
	else if (nitroFSInit("fat:/GodMode9i.nds")) nitroCurrentDrive = Drive::flashcard;
	else if (nitroFSInit("fat:/GodMode9i.dsi")) nitroCurrentDrive = Drive::flashcard;
	else {
		ownNitroFSMounted = 1;
		nitroMounted = false;
		font->print(-2, -3, false, "NitroFS init failed...", Alignment::right);
		font->update(false);
		for (int i = 0; i < 30; i++)
			swiWaitForVBlank();
	}

	if (nitroMounted && (strcmp(io_dldi_data->friendlyName, "NAND FLASH CARD LIBFATNRIO") == 0) && (*(u32*)0x02FF8000 != 0x53535A4C)) {
		FILE* file = fopen("nitro:/dldi/nrio.lz77", "rb");
		fread((void*)0x02FF8004, 1, 0x3FFC, file);
		fclose(file);

		*(u32*)0x02FF8000 = 0x53535A4C;
	}

	#ifndef _NO_BOOTSTUB_
	installBootStub(sdMounted);
	#endif

	// Ensure gm9i folder exists
	char folderPath[10];
	sprintf(folderPath, "%s:/gm9i", (sdMounted ? "sd" : "fat"));
	if ((sdMounted || flashcardMounted) && access(folderPath, F_OK) != 0)
		mkdir(folderPath, 0777);

	// Load config
	config = new Config();

	bgHide(bg3);

	// Reinit font, try to load default from SD this time
	delete font;
	if(access(config->fontPath().c_str(), F_OK) == 0)
		font = new Font(config->fontPath().c_str());
	else if(config->languageIniPath().substr(17, 2) == "zh")
		font = new Font("nitro:/fonts/misaki-gothic-8x8.frf");
	else
		font = new Font(nullptr);

	// Load translations
	langInit(false);

	if (config->languagePromptNeeded()) {
		if (!languageMenu(/*cancellable=*/false)) {
			// NitroFS unavailable — save auto-detected fallback so we don't prompt again
			config->save();
		}
	}

	keysSetRepeat(25,5);

	if (diagnosticsRequested) {
		// Console ID: the value derived during NAND mount (the 0x02F00000 buffer holds AES key3
		// material, not the plain ID; 4004D00h reads as zeros for homebrew per GBATEK).
		if (nandConsoleIDValid) {
			hwInfo.hasConsoleId = true;
			memcpy(hwInfo.consoleId, nandConsoleID, 8);
		}
		// Region + Serial come from the real NAND file — the 2FFFD68h RAM mirror is only populated
		// by the retail System Menu, which homebrew launchers bypass. Same file melonDS reads. The
		// Console ID derived above means NAND crypto is up, so open the file directly rather than
		// trusting the local nandMounted flag, which is stale at this point.
		FILE *hw = fopen("nand:/sys/HWINFO_S.dat", "rb");
		if (!hw) {
			// The nand: FAT mount can be stale here even though NAND crypto is up (Console ID
			// derived). Mount it and retry so Region/Serial don't intermittently vanish.
			nandMount();
			hw = fopen("nand:/sys/HWINFO_S.dat", "rb");
		}
		if (hw) {
			u8 region = 0xFF;
			if (fseek(hw, 0x90, SEEK_SET) == 0 && fread(&region, 1, 1, hw) == 1 && region <= 5) {
				hwInfo.hasRegion = true;
				hwInfo.region = region;
			}
			char serial[13] = {0};
			if (fseek(hw, 0x91, SEEK_SET) == 0 && fread(serial, 1, 12, hw) == 12) {
				bool ok = (serial[0] >= 0x20 && serial[0] <= 0x7E);
				for (int i = 0; ok && i < 12; i++) {
					if (serial[i] == 0) break;
					if (serial[i] < 0x20 || serial[i] > 0x7E) ok = false;
				}
				if (ok) {
					hwInfo.hasSerial = true;
					memcpy(hwInfo.serial, serial, 13);
				}
			}
			fclose(hw);
		}

		diagnosticsShow(hwInfo);
	}

	// Top bar
	font->printf(0, 0, true, Alignment::left, Palette::blackGreen, "%*c", 256 / font->width(), ' ');

	// Enable vblank handler
	irqSet(IRQ_VBLANK, vblankHandler);

	appInited = true;

	while(1) {

		if (screenMode == 0) {
			driveMenu();
		} else {
			filename = browseForFile();
		}

		if (applaunch) {
			// Construct a command line
			char filePath[PATH_MAX];
			getcwd(filePath, PATH_MAX);
			int pathLen = strlen(filePath);
			if(pathLen < PATH_MAX && filePath[pathLen - 1] != '/') {
				filePath[pathLen] = '/';
				filePath[pathLen + 1] = '\0';
				pathLen++;
			}

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
				strcpy(filePath + pathLen, name);
				free(argarray[0]);
				argarray[0] = filePath;
				font->clear(false);
				font->printf(firstCol, 0, false, alignStart, Palette::white, STR_RUNNING_X_WITH_N_PARAMETERS.c_str(), argarray[0], argarray.size());
				int err = runNdsFile(argarray[0], argarray.size(), (const char **)&argarray[0]);
				font->printf(firstCol, 1, false, alignStart, Palette::white, STR_START_FAILED_ERROR_N.c_str(), err);
			}

			if (extension(filename, {"firm"})) {
				char *name = argarray[0];
				strcpy(filePath + pathLen, name);
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
