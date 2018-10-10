#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <sys/stat.h>
#include <stdio.h>

#include "dldi-include.h"

static sNDSHeader nds;

u8 stored_SCFG_MC = 0;

bool sdMounted = false;
bool flashcardMounted = false;
bool nitroMounted = false;

bool secondaryDrive = false;				// false == SD card, true == Flashcard
bool nitroSecondaryDrive = false;			// false == SD card, true == Flashcard

char sdLabel[12];
char fatLabel[12];

void fixLabel(bool fat) {
	if (fat) {
		for (int i = 0; i < 12; i++) {
			if (((fatLabel[i] == ' ') && (fatLabel[i+1] == ' ') && (fatLabel[i+2] == ' '))
			|| ((fatLabel[i] == ' ') && (fatLabel[i+1] == ' '))
			|| (fatLabel[i] == ' ')) {
				fatLabel[i] = '\0';
				break;
			}
		}
	} else {
		for (int i = 0; i < 12; i++) {
			if (((sdLabel[i] == ' ') && (sdLabel[i+1] == ' ') && (sdLabel[i+2] == ' '))
			|| ((sdLabel[i] == ' ') && (sdLabel[i+1] == ' '))
			|| (sdLabel[i] == ' ')) {
				sdLabel[i] = '\0';
				break;
			}
		}
	}
}

bool sdFound(void) {
	if (access("sd:/", F_OK) == 0) {
		return true;
	} else {
		return false;
	}
}

bool flashcardFound(void) {
	if (access("fat:/", F_OK) == 0) {
		return true;
	} else {
		return false;
	}
}

bool bothSDandFlashcard(void) {
	if (sdMounted && flashcardMounted) {
		return true;
	} else {
		return false;
	}
}

TWL_CODE bool sdMount(void) {
	fatMountSimple("sd", get_io_dsisd());
	if (sdFound()) {
		fatGetVolumeLabel("sd", sdLabel);
		fixLabel(false);
		return true;
	}
	return false;
}

TWL_CODE void sdUnmount(void) {
	fatUnmount("sd");
	sdLabel[0] = '\0';
	sdMounted = false;
}

TWL_CODE DLDI_INTERFACE* dldiLoadFromBin (const u8 dldiAddr[]) {
	DLDI_INTERFACE* device;
	size_t dldiSize;

	// Read in the DLDI header
	if ((device = (DLDI_INTERFACE*)malloc (sizeof(DLDI_INTERFACE))) == NULL) {
		return NULL;
	}

	memcpy(device, dldiAddr, sizeof(DLDI_INTERFACE));

	// Check that it is a valid DLDI
	if (!dldiIsValid (device)) {
		free (device);
		return NULL;
	}

	// Calculate actual size of DLDI
	// Although the file may only go to the dldiEnd, the BSS section can extend past that
	if (device->dldiEnd > device->bssEnd) {
		dldiSize = (char*)device->dldiEnd - (char*)device->dldiStart;
	} else {
		dldiSize = (char*)device->bssEnd - (char*)device->dldiStart;
	}
	dldiSize = (dldiSize + 0x03) & ~0x03; 		// Round up to nearest integer multiple

	// Load entire DLDI
	free (device);
	if ((device = (DLDI_INTERFACE*)malloc (dldiSize)) == NULL) {
		return NULL;
	}
	
	memcpy(device, dldiAddr, dldiSize);

	dldiFixDriverAddresses (device);

	if (device->ioInterface.features & FEATURE_SLOT_GBA) {
		sysSetCartOwner(BUS_OWNER_ARM9);
	}
	if (device->ioInterface.features & FEATURE_SLOT_NDS) {
		sysSetCardOwner(BUS_OWNER_ARM9);
	}
	
	return device;
}

TWL_CODE bool UpdateCardInfo(sNDSHeader* nds, char* gameid, char* gamename) {
	cardReadHeader((uint8*)nds);
	memcpy(gameid, nds->gameCode, 4);
	gameid[4] = 0x00;
	memcpy(gamename, nds->gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

TWL_CODE void ShowGameInfo(const char gameid[], const char gamename[]) {
	iprintf("Game id: %s\nName:    %s", gameid, gamename);
}

TWL_CODE bool twl_flashcardMount(void) {
	if (REG_SCFG_MC != 0x11) {
		setCpuClock(false); //REG_SCFG_CLK = 0x80;	// Set NTR clock speed to avoid potential timing issues

		sysSetCardOwner (BUS_OWNER_ARM9);

		// Reset Slot-1 to allow reading title name and ID
		disableSlot1();
		for(int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
		for(int i = 0; i < 15; i++) { swiWaitForVBlank(); }

		nds.gameCode[0] = 0;
		nds.gameTitle[0] = 0;
		char gamename[13];
		char gameid[5];

		/*fifoSendValue32(FIFO_USER_04, 1);
		for (int i = 0; i < 10; i++) {
			swiWaitForVBlank();
		}
		memcpy(&nds, (void*)0x02000000, sizeof(nds));*/
		UpdateCardInfo(&nds, &gameid[0], &gamename[0]);

		/*consoleClear();
		iprintf("REG_SCFG_MC: %x\n", REG_SCFG_MC);
		ShowGameInfo(gameid, gamename);

		for (int i = 0; i < 60*5; i++) {
			swiWaitForVBlank();
		}*/

		sysSetCardOwner (BUS_OWNER_ARM7);	// 3DS fix

		// Read a DLDI driver specific to the cart
		if (!memcmp(gamename, "QMATETRIAL", 9) || !memcmp(gamename, "R4DSULTRA", 9)) {
			io_dldi_data = dldiLoadFromBin(r4idsn_sd_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} else if (!memcmp(gameid, "ACEK", 4) || !memcmp(gameid, "YCEP", 4) || !memcmp(gameid, "AHZH", 4)) {
			io_dldi_data = dldiLoadFromBin(ak2_sd_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} else if (!memcmp(gameid, "ASMA", 4)) {
			io_dldi_data = dldiLoadFromBin(r4tf_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);        
        } else if (!memcmp(gamename, "TOP TF/SD DS", 12) || !memcmp(gameid, "A76E", 4)) {
			io_dldi_data = dldiLoadFromBin(ttio_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);        
        } 

		setCpuClock(true);

		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(true);
			return true;
		}
	}
	return false;
}

bool flashcardMount(void) {
	if (flashcardFound()) {
		fatGetVolumeLabel("fat", fatLabel);
		fixLabel(true);
		return true;
	} else if (!isDSiMode()) {
		fatInitDefault();
		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(true);
			return true;
		}
		return false;
	} else {
		return twl_flashcardMount();
	}
}

void flashcardUnmount(void) {
	fatUnmount("fat");
	fatLabel[0] = '\0';
	flashcardMounted = false;
}
