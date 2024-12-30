
#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/disc_io.h>
#include "tonccpy.h"

#define SECTOR_SIZE 512

const static u8 bootSector[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SECTOR_SIZE & 0xFF, SECTOR_SIZE >> 8, 0x04, 0x01, 0x00,
	0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'R', 'A', 'M', 'D', 'R',
	'I', 'V', 'E', ' ', ' ', ' ', 'F', 'A', 'T'
};

static bool largeSize = false;
u32 ramdSectors = 0;
u8* ramdLoc = (u8*)NULL;
u8* ramdLocMep = (u8*)NULL;
const u16 bootSectorSignature = 0xAA55;

void ramd_setSize(const bool ram32MB) {
	largeSize = ram32MB;
}

bool ramd_startup() {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		ramdLoc = (u8*)malloc(0x6000 * SECTOR_SIZE);
	} else {
		ramdLoc = (u8*)calloc(0x8 * SECTOR_SIZE, 1);
		toncset(ramdLocMep, 0, (ramdSectors - 0x8) * SECTOR_SIZE); // Fill MEP with 00 to avoid displaying weird files
	}

	tonccpy(ramdLoc, bootSector, sizeof(bootSector));
	tonccpy(ramdLoc + 0x20, &ramdSectors, 4);
	tonccpy(ramdLoc + 0x1FE, &bootSectorSignature, 2);

	return true;
}

bool ramd_is_inserted() {
	return isDSiMode() || REG_SCFG_EXT != 0 || *(u16*)(0x020000C0) != 0 || *(vu16*)(0x08240000) == 1;
}

bool ramd_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	for(int i = 0; i < numSectors; i++, sector++) {
		if(isDSiMode() || REG_SCFG_EXT != 0) {
			if (largeSize) {
				if(sector >= 0xE440) {
					tonccpy(buffer + (i * SECTOR_SIZE), (void*)0x037C0000 + ((sector - 0xE440) * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0xE000) {
					tonccpy(buffer + (i * SECTOR_SIZE), (void*)0x036F8000 + ((sector - 0xE000) * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0x6000) {
					tonccpy(buffer + (i * SECTOR_SIZE), (void*)0x0D000000 + ((sector - 0x6000) * SECTOR_SIZE), SECTOR_SIZE);
				} else {
					tonccpy(buffer + (i * SECTOR_SIZE), ramdLoc + (sector * SECTOR_SIZE), SECTOR_SIZE);
				}
			} else {
				if(sector >= 0x6440) {
					tonccpy(buffer + (i * SECTOR_SIZE), (void*)0x037C0000 + ((sector - 0x6440) * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0x6000) {
					tonccpy(buffer + (i * SECTOR_SIZE), (void*)0x036F8000 + ((sector - 0x6000) * SECTOR_SIZE), SECTOR_SIZE);
				} else {
					tonccpy(buffer + (i * SECTOR_SIZE), ramdLoc + (sector * SECTOR_SIZE), SECTOR_SIZE);
				}
			}
		} else if(sector < 0x8) {
			tonccpy(buffer + (i * SECTOR_SIZE), ramdLoc + (sector * SECTOR_SIZE), SECTOR_SIZE);
		} else if(sector <= ramdSectors - 0x8) {
			tonccpy(buffer + (i * SECTOR_SIZE), ramdLocMep + ((sector - 0x8) * SECTOR_SIZE), SECTOR_SIZE);
		} else {
			return false;
		}
	}

	return true;
}

bool ramd_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	for(int i = 0; i < numSectors; i++, sector++) {
		if(isDSiMode() || REG_SCFG_EXT != 0) {
			if (largeSize) {
				if(sector >= 0xE440) {
					tonccpy((void*)0x037C0000 + ((sector - 0xE440) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0xE000) {
					tonccpy((void*)0x036F8000 + ((sector - 0xE000) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0x6000) {
					tonccpy((void*)0x0D000000 + ((sector - 0x6000) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				} else {
					tonccpy(ramdLoc + (sector * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				}
			} else {
				if(sector >= 0x6440) {
					tonccpy((void*)0x037C0000 + ((sector - 0x6440) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				} else if(sector >= 0x6000) {
					tonccpy((void*)0x036F8000 + ((sector - 0x6000) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				} else {
					tonccpy(ramdLoc + (sector * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
				}
			}
		} else if(sector < 0x8) {
			tonccpy(ramdLoc + (sector * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
		} else if(sector <= ramdSectors - 0x8) {
			tonccpy(ramdLocMep + ((sector - 0x8) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
		} else {
			return false;
		}
	}

	return true;
}

bool ramd_clear_status() {
	return true;
}

bool ramd_shutdown() {
	if(ramdLoc) {
		free(ramdLoc);
		ramdLoc = NULL;
	}

	return true;
}

const DISC_INTERFACE io_ram_drive = {
	('R' << 24) | ('A' << 16) | ('M' << 8) | '1',
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	ramd_startup,
	ramd_is_inserted,
	ramd_read_sectors,
	ramd_write_sectors,
	ramd_clear_status,
	ramd_shutdown
};
