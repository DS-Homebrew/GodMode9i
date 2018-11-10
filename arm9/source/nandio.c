
#include <nds.h>
#include <nds/disc_io.h>
#include <malloc.h>
//#include "../term256/term256ext.h"
//#include "crypto.h"

#define SECTOR_SIZE 512
#define CRYPT_BUF_LEN 64

static u8* crypt_buf = 0;

static u32 fat_sig_fix_offset = 0;

TWL_CODE void nandio_set_fat_sig_fix(u32 offset) {
	fat_sig_fix_offset = offset;
}

TWL_CODE bool nandio_startup() {
	if (crypt_buf == 0) {
		crypt_buf = (u8*)memalign(32, SECTOR_SIZE * CRYPT_BUF_LEN);
		//if (crypt_buf == 0) {
			//prt("nandio: failed to alloc buffer\n");
		//}
	}
	return crypt_buf != 0;
}

TWL_CODE bool nandio_is_inserted() {
	return true;
}

// len is guaranteed <= CRYPT_BUF_LEN
TWL_CODE static bool read_sectors(sec_t start, sec_t len, void *buffer) {
	if (nand_ReadSectors(start, len, crypt_buf)) {
		//dsi_nand_crypt(buffer, crypt_buf, start * SECTOR_SIZE / AES_BLOCK_SIZE, len * SECTOR_SIZE / AES_BLOCK_SIZE);
		if (fat_sig_fix_offset &&
			start == fat_sig_fix_offset
			&& ((u8*)buffer)[0x36] == 0
			&& ((u8*)buffer)[0x37] == 0
			&& ((u8*)buffer)[0x38] == 0)
		{
			((u8*)buffer)[0x36] = 'F';
			((u8*)buffer)[0x37] = 'A';
			((u8*)buffer)[0x38] = 'T';
		}
		return true;
	} else {
		return false;
	}
}

TWL_CODE bool nandio_read_sectors(sec_t offset, sec_t len, void *buffer) {
	// iprintf("R: %u(0x%08x), %u\n", (unsigned)offset, (unsigned)offset, (unsigned)len);
	while (len >= CRYPT_BUF_LEN) {
		if (!read_sectors(offset, CRYPT_BUF_LEN, buffer)) {
			return false;
		}
		offset += CRYPT_BUF_LEN;
		len -= CRYPT_BUF_LEN;
		buffer = ((u8*)buffer) + SECTOR_SIZE * CRYPT_BUF_LEN;
	}
	if (len > 0) {
		return read_sectors(offset, len, buffer);
	} else {
		return true;
	}
}

TWL_CODE static bool write_sectors(sec_t start, sec_t len, const void *buffer) {
	// NAND writes disabled to avoid FAT mismatch
	/*dsi_nand_crypt(crypt_buf, buffer, start * SECTOR_SIZE / AES_BLOCK_SIZE, len * SECTOR_SIZE / AES_BLOCK_SIZE);
	if(nand_WriteSectors(start, len, crypt_buf)){
		return true;
	} else {*/
		return false;
	//}
}

TWL_CODE bool nandio_write_sectors(sec_t offset, sec_t len, const void *buffer) {
	// iprintf("W: %u(0x%08x), %u\n", (unsigned)offset, (unsigned)offset, (unsigned)len);
	/*while (len >= CRYPT_BUF_LEN) {
		if (!write_sectors(offset, CRYPT_BUF_LEN, buffer)) {*/
			return false;
	/*	}
		offset += CRYPT_BUF_LEN;
		len -= CRYPT_BUF_LEN;
		buffer = ((u8*)buffer) + SECTOR_SIZE * CRYPT_BUF_LEN;
	}
	if (len > 0) {
		return write_sectors(offset, len, buffer);
	} else {
		return true;
	}*/
}

TWL_CODE bool nandio_clear_status() {
	return true;
}

TWL_CODE bool nandio_shutdown() {
	free(crypt_buf);
	crypt_buf = 0;
	return true;
}

const DISC_INTERFACE io_dsi_nand = {
	('N' << 24) | ('A' << 16) | ('N' << 8) | 'D',
	//FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	FEATURE_MEDIUM_CANREAD,
	nandio_startup,
	nandio_is_inserted,
	nandio_read_sectors,
	nandio_write_sectors,
	nandio_clear_status,
	nandio_shutdown
};
