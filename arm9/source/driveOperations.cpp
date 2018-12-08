#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>

#include "main.h"
#include "crypto.h"
#include "sector0.h"
#include "nandio.h"
#include "dldi-include.h"

static sNDSHeader nds;

u8 stored_SCFG_MC = 0;

static bool slot1Enabled = true;

int nand_is3DS;
bool (*read_raw_sectors)(sec_t, sec_t, void*) = 0;
bool nandMounted = false;
bool sdMounted = false;
bool sdMountedDone = false;				// true if SD mount is successful once
bool flashcardMounted = false;
bool nitroMounted = false;

bool secondaryDrive = false;				// false == SD card, true == Flashcard
bool nitroSecondaryDrive = false;			// false == SD card, true == Flashcard

char sdLabel[12];
char fatLabel[12];

int sdSize = 0;
int fatSize = 0;

static u32 sector_buf32[SECTOR_SIZE/sizeof(u32)];
static u8 *sector_buf = (u8*)sector_buf32;

static ssize_t nand_size;

static u32 emmc_cid32[4];
static u8 *emmc_cid = (u8*)emmc_cid32;
static u32 console_id32[2];
static u8 *console_id = (u8*)console_id32;

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

int get_ids() {
	if (!isDSiMode()) {
		return -2;
	}

	fifoSendValue32(FIFO_USER_01, 1);
	while (!fifoCheckValue32(FIFO_USER_01)) swiIntrWait(1, IRQ_FIFO_NOT_EMPTY);
	int ret = fifoGetValue32(FIFO_USER_01);
	if (ret) {
		return -3;
	}

	nand_size = nand_GetSize();
	if (nand_size == 0) {
		return -3;
	}

	fifoSendValue32(FIFO_USER_01, 4);
	while (fifoCheckDatamsgLength(FIFO_USER_01) < 16) swiIntrWait(1, IRQ_FIFO_NOT_EMPTY);
	fifoGetDatamsg(FIFO_USER_01, 16, (u8*)emmc_cid);

	fifoSendValue32(FIFO_USER_01, 5);
	while (fifoCheckDatamsgLength(FIFO_USER_01) < 8) swiIntrWait(1, IRQ_FIFO_NOT_EMPTY);
	fifoGetDatamsg(FIFO_USER_01, 8, console_id);

	return 0;
}

TWL_CODE int test_sector0(int *p_is3DS) {
	int is3DS = parse_ncsd(sector_buf, 0) == 0;
	// iprintf("sector 0 is %s\n", is3DS ? "3DS" : "DSi");
	dsi_crypt_init(console_id, emmc_cid, is3DS);
	dsi_nand_crypt(sector_buf, sector_buf, 0, SECTOR_SIZE / AES_BLOCK_SIZE);
	if (p_is3DS) {
		*p_is3DS = is3DS;
	}
	return parse_mbr(sector_buf, is3DS, 0);
}

TWL_CODE bool nandMount(void) {
	nand_ReadSectors(0, 1, sector_buf);

	test_sector0(&nand_is3DS);
	mbr_t *mbr = (mbr_t*)sector_buf;
	int mnt_ret;

	nandio_set_fat_sig_fix(nand_is3DS ? 0 : mbr->partitions[0].offset);
	mnt_ret = fatMount("nand", &io_dsi_nand, mbr->partitions[0].offset, 4, 64);
	if (mnt_ret == 0) {
		return false;
	}
	read_raw_sectors = nand_ReadSectors;
	return true;
}

TWL_CODE bool sdMount(void) {
	fatMountSimple("sd", get_io_dsisd());
	if (sdFound()) {
		sdMountedDone = true;
		fatGetVolumeLabel("sd", sdLabel);
		fixLabel(false);
		/*struct statvfs st;
		if (statvfs("sd:/", &st) == 0) {
			sdSize = st.f_bsize * st.f_blocks;
		}*/
		return true;
	}
	return false;
}

TWL_CODE void sdUnmount(void) {
	fatUnmount("sd");
	sdLabel[0] = '\0';
	sdSize = 0;
	sdMounted = false;
}

TWL_CODE void nandUnmount(void) {
	fatUnmount("nand");
	nand_size = 0;
	nandMounted = false;
}

TWL_CODE DLDI_INTERFACE* dldiLoadFromBin (const u8 dldiAddr[]) {
	// Check that it is a valid DLDI
	if (!dldiIsValid ((DLDI_INTERFACE*)dldiAddr)) {
		return NULL;
	}

	DLDI_INTERFACE* device = (DLDI_INTERFACE*)dldiAddr;
	size_t dldiSize;

	// Calculate actual size of DLDI
	// Although the file may only go to the dldiEnd, the BSS section can extend past that
	if (device->dldiEnd > device->bssEnd) {
		dldiSize = (char*)device->dldiEnd - (char*)device->dldiStart;
	} else {
		dldiSize = (char*)device->bssEnd - (char*)device->dldiStart;
	}
	dldiSize = (dldiSize + 0x03) & ~0x03; 		// Round up to nearest integer multiple
	
	// Clear unused space
	memset(device+dldiSize, 0, 0x4000-dldiSize);

	dldiFixDriverAddresses (device);

	if (device->ioInterface.features & FEATURE_SLOT_GBA) {
		sysSetCartOwner(BUS_OWNER_ARM9);
	}
	if (device->ioInterface.features & FEATURE_SLOT_NDS) {
		sysSetCardOwner(BUS_OWNER_ARM9);
	}
	
	return device;
}

TWL_CODE bool UpdateCardInfo(char* gameid, char* gamename) {
	cardReadHeader((uint8*)0x02000000);
	memcpy(&nds, (void*)0x02000000, sizeof(sNDSHeader));
	memcpy(gameid, &nds.gameCode, 4);
	gameid[4] = 0x00;
	memcpy(gamename, &nds.gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

TWL_CODE void ShowGameInfo(const char gameid[], const char gamename[]) {
	iprintf("Game id: %s\nName:    %s", gameid, gamename);
}

TWL_CODE bool twl_flashcardMount(void) {
	if (REG_SCFG_MC != 0x11) {
		sysSetCardOwner (BUS_OWNER_ARM9);

		// Reset Slot-1 to allow reading title name and ID
		if (slot1Enabled) {
			disableSlot1();
			for(int i = 0; i < 25; i++) { swiWaitForVBlank(); }
			slot1Enabled = false;
		}
		if (appInited) {
			for(int i = 0; i < 35; i++) { swiWaitForVBlank(); }	// Make sure cart is inserted correctly
		}
		if (REG_SCFG_MC == 0x11) {
			sysSetCardOwner (BUS_OWNER_ARM7);
			return false;
		}
		if (!slot1Enabled) {
			enableSlot1();
			for(int i = 0; i < 15; i++) { swiWaitForVBlank(); }
			slot1Enabled = true;
		}

		nds.gameCode[0] = 0;
		nds.gameTitle[0] = 0;
		char gamename[13];
		char gameid[5];

		/*fifoSendValue32(FIFO_USER_04, 1);
		for (int i = 0; i < 10; i++) {
			swiWaitForVBlank();
		}
		memcpy(&nds, (void*)0x02000000, sizeof(nds));*/
		UpdateCardInfo(&gameid[0], &gamename[0]);

		/*consoleClear();
		iprintf("REG_SCFG_MC: %x\n", REG_SCFG_MC);
		ShowGameInfo(gameid, gamename);

		for (int i = 0; i < 60*5; i++) {
			swiWaitForVBlank();
		}*/

		sysSetCardOwner (BUS_OWNER_ARM7);	// 3DS fix

		if (gameid[0] >= 0x00 && gameid[0] < 0x20) {
			return false;
		}

		// Read a DLDI driver specific to the cart
		if (!memcmp(gameid, "ASMA", 4)) {
			io_dldi_data = dldiLoadFromBin(r4tf_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);      
		} else if (!memcmp(gamename, "TOP TF/SD DS", 12) || !memcmp(gameid, "A76E", 4)) {
			io_dldi_data = dldiLoadFromBin(tt_sd_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
 		} else /*if (!memcmp(gamename, "PASS", 4) && !memcmp(gameid, "ASME", 4)) {
			io_dldi_data = dldiLoadFromBin(CycloEvo_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} else*/ if (!memcmp(gamename, "D!S!XTREME", 12) && !memcmp(gameid, "AYIE", 4)) {
			io_dldi_data = dldiLoadFromBin(dsx_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface); 
        } else if (!memcmp(gamename, "QMATETRIAL", 9) || !memcmp(gamename, "R4DSULTRA", 9)) {
			io_dldi_data = dldiLoadFromBin(r4idsn_sd_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} else if (!memcmp(gameid, "ACEK", 4) || !memcmp(gameid, "YCEP", 4) || !memcmp(gameid, "AHZH", 4)) {
			io_dldi_data = dldiLoadFromBin(ak2_sd_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		} /*else if (!memcmp(gameid, "ALXX", 4)) {
			io_dldi_data = dldiLoadFromBin(dstwo_dldi);
			fatMountSimple("fat", &io_dldi_data->ioInterface);
		}*/

		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(true);
			/*struct statvfs st;
			if (statvfs("fat:/", &st) == 0) {
				fatSize = st.f_bsize * st.f_blocks;
			}*/
			return true;
		}
	}
	return false;
}

bool flashcardMount(void) {
	if ((!isDSiMode()) || (arm7SCFGLocked && !sdMountedDone)) {
		fatInitDefault();
		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(true);
			/*struct statvfs st;
			if (statvfs("fat:/", &st) == 0) {
				fatSize = st.f_bsize * st.f_blocks;
			}*/
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
	fatSize = 0;
	flashcardMounted = false;
}
