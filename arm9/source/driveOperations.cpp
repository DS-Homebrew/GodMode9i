#include "driveOperations.h"

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "main.h"
#include "dldi-include.h"
#include "lzss.h"
#include "ramd.h"
#include "my_sd.h"
#include "nandio.h"
#include "imgio.h"
#include "tonccpy.h"
#include "language.h"

#include "io_m3_common.h"
#include "io_g6_common.h"
#include "io_sc_common.h"
#include "exptools.h"

static sNDSHeader nds;

u8 stored_SCFG_MC = 0;

static bool slot1Enabled = true;

bool nandMounted = false;
bool sdMounted = false;
bool sdMountedDone = false;				// true if SD mount is successful once
bool flashcardMounted = false;
bool ramdriveMounted = false;
bool imgMounted = false;
bool nitroMounted = false;

Drive currentDrive = Drive::sdCard;
Drive nitroCurrentDrive = Drive::sdCard;
Drive imgCurrentDrive = Drive::sdCard;

char sdLabel[12];
char fatLabel[12];
char imgLabel[12];

u32 nandSize = 0;
u64 sdSize = 0;
u64 fatSize = 0;
u64 imgSize = 0;
u32 ramdSize = 0;

static float getGbNumber(u64 bytes) {
	float gbNumber = 0.0f;
	for (u64 i = 0; i <= bytes; i += 0x6666666) {
		gbNumber += 0.1f;
	}
	return gbNumber;
}

static float getTbNumber(u64 bytes) {
	float tbNumber = 0.0f;
	for (u64 i = 0; i <= bytes; i += 0x1999999999) {
		tbNumber += 0.01f;
	}
	return tbNumber;
}

std::string getDriveBytes(u64 bytes)
{
	char buffer[32];
	if (bytes < (1024 * 1024))
		sniprintf(buffer, sizeof(buffer), STR_N_KB.c_str(), (int)bytes >> 10);

	else if (bytes >= (1024 * 1024) && bytes < (1024 * 1024 * 1024))
		sniprintf(buffer, sizeof(buffer), STR_N_MB.c_str(), (int)bytes >> 20);

	else if (bytes >= 0x40000000 && bytes < 0x10000000000)
		snprintf(buffer, sizeof(buffer), STR_N_GB_FLOAT.c_str(), getGbNumber(bytes));

	else
		snprintf(buffer, sizeof(buffer), STR_N_TB_FLOAT.c_str(), getTbNumber(bytes));

	return buffer;
}

const char* getDrivePath(void) {
	switch (currentDrive) {
		case Drive::sdCard:
			return "sd:/";
		case Drive::flashcard:
			return "fat:/";
		case Drive::ramDrive:
			return "ram:/";
		case Drive::nand:
			return "nand:/";
		case Drive::nitroFS:
			return "nitro:/";
		case Drive::fatImg:
			return "img:/";
	}
	return "";
}

void fixLabel(char* label) {
	for (int i = strlen(label) - 1; i >= 0; i--) {
		if(label[i] != ' ') {
			label[i + 1] = '\0';
			break;
		}
	}
}

bool nandFound(void) {
	return (access("nand:/", F_OK) == 0);
}

bool sdFound(void) {
	return (access("sd:/", F_OK) == 0);
}

bool flashcardFound(void) {
	return (access("fat:/", F_OK) == 0);
}

bool bothSDandFlashcard(void) {
	if (sdMounted && flashcardMounted) {
		return true;
	} else {
		return false;
	}
}

bool imgFound(void) {
	return (access("img:/", F_OK) == 0);
}

TWL_CODE bool nandMount(void) {
	fatMountSimple("nand", &io_dsi_nand);
	if (nandFound()) {
		struct statvfs st;
		if (statvfs("nand:/", &st) == 0) {
			nandSize = st.f_bsize * st.f_blocks;
		}
		return true;
	}
	return false;
}

TWL_CODE void nandUnmount(void) {
	fatUnmount("nand");
	nandSize = 0;
	nandMounted = false;
}

bool sdMount(void) {
	fatMountSimple("sd", __my_io_dsisd());
	if (sdFound()) {
		sdMountedDone = true;
		fatGetVolumeLabel("sd", sdLabel);
		fixLabel(sdLabel);
		struct statvfs st;
		if (statvfs("sd:/", &st) == 0) {
			sdSize = st.f_bsize * st.f_blocks;
		}
		return true;
	}
	return false;
}

u64 getBytesFree(const char* drivePath) {
    struct statvfs st;
    statvfs(drivePath, &st);
    return (u64)st.f_bsize * (u64)st.f_bavail;
}

void sdUnmount(void) {
	fatUnmount("sd");
	my_sdio_Shutdown();
	sdLabel[0] = '\0';
	sdSize = 0;
	sdMounted = false;
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
	toncset(device+dldiSize, 0, 0x4000-dldiSize);

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
	tonccpy(&nds, (void*)0x02000000, sizeof(sNDSHeader));
	tonccpy(gameid, &nds.gameCode, 4);
	gameid[4] = 0x00;
	tonccpy(gamename, &nds.gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

const DISC_INTERFACE *dldiGet(void) {
	if(io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA)
		sysSetCartOwner(BUS_OWNER_ARM9);
	if(io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS)
		sysSetCardOwner(BUS_OWNER_ARM9);

	return &io_dldi_data->ioInterface;
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

		UpdateCardInfo(&gameid[0], &gamename[0]);

		sysSetCardOwner (BUS_OWNER_ARM7);	// 3DS fix

		if (gameid[0] >= 0x00 && gameid[0] < 0x20) {
			return false;
		}

		// Read a DLDI driver specific to the cart
		if (!memcmp(gameid, "ASMA", 4)) {
			io_dldi_data = dldiLoadFromBin(r4tf_dldi);
			fatMountSimple("fat", dldiGet());
		} else if (!memcmp(gamename, "TOP TF/SD DS", 12) || !memcmp(gameid, "A76E", 4)) {
			io_dldi_data = dldiLoadFromBin(tt_sd_dldi);
			fatMountSimple("fat", dldiGet());
 		} else /*if (!memcmp(gamename, "PASS", 4) && !memcmp(gameid, "ASME", 4)) {
			io_dldi_data = dldiLoadFromBin(CycloEvo_dldi);
			fatMountSimple("fat", dldiGet());
		} else*/ if (!memcmp(gamename, "D!S!XTREME", 10) && !memcmp(gameid, "AYIE", 4)) {
			io_dldi_data = dldiLoadFromBin(dsx_dldi);
			fatMountSimple("fat", dldiGet()); 
        } else if (!memcmp(gamename, "QMATETRIAL", 9) || !memcmp(gamename, "R4DSULTRA", 9)) {
			io_dldi_data = dldiLoadFromBin(r4idsn_sd_dldi);
			fatMountSimple("fat", dldiGet());
		} else if (!memcmp(gameid, "ACEK", 4) || !memcmp(gameid, "YCEP", 4) || !memcmp(gameid, "AHZH", 4) || !memcmp(gameid, "CHPJ", 4) || !memcmp(gameid, "ADLP", 4)) {
			io_dldi_data = dldiLoadFromBin(ak2_sd_dldi);
			fatMountSimple("fat", dldiGet());
		} /*else if (!memcmp(gameid, "ALXX", 4)) {
			io_dldi_data = dldiLoadFromBin(dstwo_dldi);
			fatMountSimple("fat", dldiGet());
		}*/

		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(fatLabel);
			struct statvfs st;
			if (statvfs("fat:/", &st) == 0) {
				fatSize = st.f_bsize * st.f_blocks;
			}
			return true;
		}
	}
	return false;
}

bool flashcardMount(void) {
	if (!isDSiMode() || (arm7SCFGLocked && !sdMountedDone)) {
		fatInitDefault();
		if (flashcardFound()) {
			fatGetVolumeLabel("fat", fatLabel);
			fixLabel(fatLabel);
			struct statvfs st;
			if (statvfs("fat:/", &st) == 0) {
				fatSize = st.f_bsize * st.f_blocks;
			}
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

void ramdriveMount(bool ram32MB) {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		ramdSectors = ram32MB ? 0xE000 : 0x6000;

		fatMountSimple("ram", &io_ram_drive);
	} else if (isRegularDS) {
		ramdSectors = 0x8 + 0x4000;
		ramdLocMep = (u8*)0x09000000;

		if (*(u16*)(0x020000C0) != 0x334D && *(u16*)(0x020000C0) != 0x3647 && *(u16*)(0x020000C0) != 0x4353 && *(u16*)(0x020000C0) != 0x5A45) {
			*(u16*)(0x020000C0) = 0;	// Clear Slot-2 flashcard flag
		}

		if (*(u16*)(0x020000C0) == 0) {
			*(vu16*)(0x08000000) = 0x4D54;	// Write test
			if (*(vu16*)(0x08000000) != 0x4D54) {	// If not writeable
				_M3_changeMode(M3_MODE_RAM);	// Try again with M3
				*(u16*)(0x020000C0) = 0x334D;
				*(vu16*)(0x08000000) = 0x4D54;
			}
			if (*(vu16*)(0x08000000) != 0x4D54) {
				_G6_SelectOperation(G6_MODE_RAM);	// Try again with G6
				*(u16*)(0x020000C0) = 0x3647;
				*(vu16*)(0x08000000) = 0x4D54;
			}
			if (*(vu16*)(0x08000000) != 0x4D54) {
				_SC_changeMode(SC_MODE_RAM);	// Try again with SuperCard
				*(u16*)(0x020000C0) = 0x4353;
				*(vu16*)(0x08000000) = 0x4D54;
			}
			if (*(vu16*)(0x08000000) != 0x4D54) {
				cExpansion::SetRompage(381);	// Try again with EZ Flash
				cExpansion::OpenNorWrite();
				cExpansion::SetSerialMode();
				*(u16*)(0x020000C0) = 0x5A45;
				*(vu16*)(0x08000000) = 0x4D54;
			}
			if (*(vu16*)(0x08000000) != 0x4D54) {
				*(u16*)(0x020000C0) = 0;
				*(vu16*)(0x08240000) = 1; // Try again with Nintendo Memory Expansion Pak
			}
		}

		if (*(u16*)(0x020000C0) == 0x334D || *(u16*)(0x020000C0) == 0x3647 || *(u16*)(0x020000C0) == 0x4353) {
			ramdLocMep = (u8*)0x08000000;
			ramdSectors = 0x8 + 0x10000;
		} else if (*(u16*)(0x020000C0) == 0x5A45) {
			ramdLocMep = (u8*)0x08000000;
			ramdSectors = 0x8 + 0x8000;
		}

		if (*(u16*)(0x020000C0) != 0 || *(vu16*)(0x08240000) == 1) {
			fatMountSimple("ram", &io_ram_drive);
		}
	}

	ramdriveMounted = (access("ram:/", F_OK) == 0);

	if (ramdriveMounted) {
		struct statvfs st;
		if (statvfs("ram:/", &st) == 0) {
			ramdSize = st.f_bsize * st.f_blocks;
		}
	}
}

void nitroUnmount(void) {
	fatUnmount("nitro");
	nitroMounted = false;
}

bool imgMount(const char* imgName) {
	extern const char* currentImgName;

	currentImgName = imgName;
	fatMountSimple("img", &io_img);
	if (imgFound()) {
		fatGetVolumeLabel("img", imgLabel);
		fixLabel(imgLabel);
		struct statvfs st;
		if (statvfs("img:/", &st) == 0) {
			imgSize = st.f_bsize * st.f_blocks;
		}
		return true;
	}
	return false;
}

void imgUnmount(void) {
	fatUnmount("img");
	imgLabel[0] = '\0';
	imgSize = 0;
	imgMounted = false;
}

bool driveWritable(Drive drive) {
	switch(drive) {
		case Drive::sdCard:
			return __my_io_dsisd()->features & FEATURE_MEDIUM_CANWRITE;
		case Drive::flashcard:
			return dldiGet()->features & FEATURE_MEDIUM_CANWRITE;
		case Drive::ramDrive:
			return io_ram_drive.features & FEATURE_MEDIUM_CANWRITE;
		case Drive::nand:
			return io_dsi_nand.features & FEATURE_MEDIUM_CANWRITE;
		case Drive::nitroFS:
			return false;
		case Drive::fatImg:
			return io_img.features & FEATURE_MEDIUM_CANWRITE;
	}

	return false;
}
