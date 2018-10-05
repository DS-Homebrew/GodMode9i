#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <sys/stat.h>
#include <stdio.h>

#include "r4idsn_sd_bin.h"
#include "ak2_sd_bin.h"

static sNDSHeader nds;

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
	if ((access("sd:/", F_OK) == 0) && (access("fat:/", F_OK) == 0)) {
		return true;
	} else {
		return false;
	}
}

DLDI_INTERFACE* dldiLoadFromBin (const u8 dldiAddr[]) {
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

bool UpdateCardInfo(sNDSHeader* nds, char* gameid, char* gamename) {
	cardReadHeader((uint8*)nds);
	memcpy(gameid, nds->gameCode, 4);
	gameid[4] = 0x00;
	memcpy(gamename, nds->gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

void ShowGameInfo(const char gameid[], const char gamename[]) {
	iprintf("Game id: %s\nName:    %s", gameid, gamename);
}

void flashcardMount(void) {
	if (!flashcardFound() && REG_SCFG_MC != 0x11) {
		// Reset Slot-1 to allow reading title name and ID
		sysSetCardOwner (BUS_OWNER_ARM9);
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

		/*SetBrightness(0, 0);
		SetBrightness(1, 0);
		consoleDemoInit();
		iprintf("REG_SCFG_MC: %x\n", REG_SCFG_MC);
		ShowGameInfo(gameid, gamename);

		for (int i = 0; i < 60*5; i++) {
			swiWaitForVBlank();
		}*/

		sysSetCardOwner (BUS_OWNER_ARM7);	// 3DS fix

		// Read a DLDI driver specific to the cart
		if (!memcmp(gamename, "R4DSULTRA", 9)) {
			io_dldi_data = dldiLoadFromBin(r4idsn_sd_bin);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} else if (!memcmp(gameid, "YCEP", 4) || !memcmp(gameid, "AHZH", 4)) {
			io_dldi_data = dldiLoadFromBin(ak2_sd_bin);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		}
	}
}