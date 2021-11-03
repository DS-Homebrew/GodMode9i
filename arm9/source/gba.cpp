/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo. EZFlash 3-in-1 is a trademark by EZFlash.
 *
 * gba.cpp: Functions for working with the GBA-slot on a Nintendo DS.
 *    EZFlash 3-in-1 functions are found in dsCard.h/.cpp
 *
 * Copyright (C) Pokedoc (2010)
 */
/* 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <nds.h>
#include <fat.h>

#include <sys/iosupport.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <algorithm>
#include <nds/dma.h>


#include <dirent.h>

#include "gba.h"
// #include "dsCard.h"

// #include "display.h"
// #include "globals.h"
// #include "strings.h"

inline u32 min(u32 i, u32 j) { return (i < j) ? i : j;}
inline u32 max(u32 i, u32 j) { return (i > j) ? i : j;}



// -----------------------------------------------------
#define MAGIC_EEPR 0x52504545
#define MAGIC_SRAM 0x4d415253
#define MAGIC_FLAS 0x53414c46

#define MAGIC_H1M_ 0x5f4d3148

#define EEPROM_ADDRESS (0x0DFFFF00)
#define REG_EEPROM *(vu16 *)(EEPROM_ADDRESS)


// -----------------------------------------------------------
bool gbaIsGame()
{
	// look for some magic bytes of the compressed Nintendo logo
	uint32 *data = (uint32*)0x08000004;
	
	if (*data == 0x51aeff24) {
		data ++; data ++;
		if (*data == 0x0a82843d)
			return true;
	}
	return false;
}

void EEPROM_SendPacket(u16 *packet, int size)
{
	REG_EXMEMCNT = (REG_EXMEMCNT & 0xFFE3) | 0x000C;
	DMA3_SRC = (u32)packet;
	DMA3_DEST = EEPROM_ADDRESS;
	DMA3_CR = 0x80000000 + size;
	while((DMA3_CR & 0x80000000) != 0);
}

void EEPROM_ReceivePacket(u16 *packet, int size)
{
	REG_EXMEMCNT = (REG_EXMEMCNT & 0xFFE3) | 0x000C;
	DMA3_SRC = EEPROM_ADDRESS;
	DMA3_DEST = (u32)packet;
	DMA3_CR = 0x80000000 + size;
	while((DMA3_CR & 0x80000000) != 0);
}

// local function
void gbaEepromRead8Bytes(u8 *out, u16 addr, bool short_addr)
{
	u16 packet[68];

	memset(packet, 0, 68 * 2);

	// Read request
	packet[0] = 1;
	packet[1] = 1;

	// 6 or 14 bytes eeprom address (MSB first)
	for(int i = 2, shift = (short_addr ? 5 : 13); i < (short_addr ? 8 : 16); i++, shift--) {
		packet[i] = (addr >> shift) & 1;
	}

	// End of request
	packet[short_addr ? 8 : 16] = 0;

	// Do transfers
	EEPROM_SendPacket(packet, short_addr ? 9 : 17);
	memset(packet, 0, 68 * 2);
	EEPROM_ReceivePacket(packet, 68);

	// Extract data
	u16 *in_pos = &packet[4];
	for(int byte = 7; byte >= 0; --byte) {
		u8 out_byte = 0;
		for(int bit = 7; bit >= 0; --bit) {
			// out_byte += (*in_pos++) << bit;
			out_byte += ((*in_pos++) & 1) << bit;
		}
		*out++ = out_byte;
	}
}

// local function
void gbaEepromWrite8Bytes(u8 *in, u16 addr, bool short_addr = false)
{
	u16 packet_length = short_addr ? 73 : 81;
	u16 packet[packet_length];

	memset( packet, 0, packet_length * 2);

	// Write request
	packet[0] = 1;
	packet[1] = 0;

	// 6 or 14 bytes eeprom address (MSB first)
	for(int i = 2, shift = (short_addr ? 5 : 13); i < (short_addr ? 8 : 16); i++, shift--) {
		packet[i] = (addr >> shift) & 1;
	}

	// Extract data
	u16 *out_pos = &packet[short_addr ? 8 : 16];
	for(int byte = 7; byte >= 0; --byte) {
		u8 in_byte = *in++;
		for(int bit = 7; bit >= 0; --bit) {
			*out_pos++ = (in_byte >> bit) & 1;
		}
	}

	// End of request
	packet[packet_length - 1] = 0;

	// Do transfers
	EEPROM_SendPacket(packet, packet_length);

	// Wait for EEPROM to finish (should timeout after 10 ms)
	while((REG_EEPROM & 1) == 0);
}

saveTypeGBA gbaGetSaveType() {
	// Search for any one of the magic version strings in the ROM. They are always dword-aligned.
	uint32 *data = (uint32*)0x08000000;
	
	for (int i = 0; i < (0x02000000 >> 2); i++, data++) {
		if (*data == MAGIC_EEPR) {
			u8 *buf = new u8[0x2000];
			u8 *ptr = buf;
			for (int j = 0; j < 0x400; j++, ptr += 8) {
				gbaEepromRead8Bytes(ptr, j, false);
				for(int sleep=0;sleep<512000;sleep++);
			}
			for(int j = 8; j < 0x800; j += 8) {
				if(memcmp(buf, buf + j, 8) != 0) {
					delete[] buf;
					return SAVE_GBA_EEPROM_8;
				}
			}
			delete[] buf;
			return SAVE_GBA_EEPROM_05;
		} else if (*data == MAGIC_SRAM) {
			// *always* 32 kB
			return SAVE_GBA_SRAM_32;
		} else if (*data == MAGIC_FLAS) {
			// 64 kB oder 128 kB
			uint32 *data2 = data + 1;
			if (*data2 == MAGIC_H1M_)
				return SAVE_GBA_FLASH_128;
			else
				return SAVE_GBA_FLASH_64;
		}
	}

	return SAVE_GBA_NONE;
}

uint32 gbaGetSaveSizeLog2(saveTypeGBA type)
{
	if (type == SAVE_GBA_NONE)
		type = gbaGetSaveType();
	
	switch (type) {
		case SAVE_GBA_EEPROM_05:
			return 9;
		case SAVE_GBA_EEPROM_8:
			return 13;
		case SAVE_GBA_SRAM_32:
			return 15;
		case SAVE_GBA_FLASH_64:
			return 16;
		case SAVE_GBA_FLASH_128:
			return 17;
		case SAVE_GBA_NONE:
		default:
			return 0;
	}
}

uint32 gbaGetSaveSize(saveTypeGBA type)
{
	if (type == SAVE_GBA_NONE)
		return 0;
	else
		return 1 << gbaGetSaveSizeLog2(type);
}

bool gbaReadSave(u8 *dst, u32 src, u32 len, saveTypeGBA type)
{
	int nbanks = 2; // for type 4,5
	bool eeprom_long = true;
	
	switch (type) {
	case SAVE_GBA_EEPROM_05: {
		eeprom_long = false;
		}
	case SAVE_GBA_EEPROM_8: {
		int start, end;
		start = src >> 3;
		end = (src + len) >> 3;
		u8 *ptr = dst;
		for (int j = start; j < end; j++, ptr += 8) {
			gbaEepromRead8Bytes(ptr, j, !eeprom_long);
			for(int sleep=0;sleep<512000;sleep++);
		}
		break;
		}
	case SAVE_GBA_SRAM_32: {
		// SRAM: blind copy
		int start = 0x0a000000 + src;
		u8 *tmpsrc = (u8*)start;
		sysSetBusOwners(true, true);
		for (u32 i = 0; i < len; i++, tmpsrc++, dst++)
			*dst = *tmpsrc;
		break;
		}
	case SAVE_GBA_FLASH_64:
		// FLASH - must be opend by register magic, then blind copy
		nbanks = 1;
	case SAVE_GBA_FLASH_128:
		for (int j = 0; j < nbanks; j++) {
			// we need to wait a few cycles before the hardware reacts!
			*(vu8*)0x0a005555 = 0xaa;
			swiDelay(10);
			*(vu8*)0x0a002aaa = 0x55;
			swiDelay(10);
			*(vu8*)0x0a005555 = 0xb0;
			swiDelay(10);
			*(vu8*)0x0a000000 = (u8)j;
			swiDelay(10);
			u32 start, sublen;
			if (j == 0) {
				start = 0x0a000000 + src;
				sublen = (src < 0x10000) ? min(len, (1 << 16) - src) : 0;
			} else if (j == 1) {
				start = max(0x09ff0000 + src, 0x0a000000);
				sublen = (src + len < 0x10000) ? 0 : min(len, len - (0x10000 - src));
			}
			u8 *tmpsrc = (u8*)start;
			sysSetBusOwners(true, true);
			for (u32 i = 0; i < sublen; i++, tmpsrc++, dst++)
				*dst = *tmpsrc;
		}
		break;
	case SAVE_GBA_NONE:
		break;
	}
	return true;
}

bool gbaIsAtmel()
{
	*(vu8*)0x0a005555 = 0xaa;
	swiDelay(10);
	*(vu8*)0x0a002aaa = 0x55;
	swiDelay(10);
	*(vu8*)0x0a005555 = 0x90; // ID mode
	swiDelay(10);
	//
	u8 dev = *(u8*)0x0a000001;
	u8 man = *(u8*)0x0a000000;
	//
	*(vu8*)0x0a005555 = 0xaa;
	swiDelay(10);
	*(vu8*)0x0a002aaa = 0x55;
	swiDelay(10);
	*(vu8*)0x0a005555 = 0xf0; // leave ID mode
	swiDelay(10);
	//
	//char txt[128];
	// sprintf(txt, "Man: %x, Dev: %x", man, dev);
	// displayStateF(STR_STR, txt);
	if ((man == 0x3d) && (dev == 0x1f))
		return true;
	else
		return false;
}

bool gbaWriteSave(u32 dst, u8 *src, u32 len, saveTypeGBA type)
{
	int nbanks = 2; // for type 4,5
	bool eeprom_long = true;
	
	switch (type) {
	case SAVE_GBA_EEPROM_05: {
		eeprom_long = false;
		}
	case SAVE_GBA_EEPROM_8: {
	/*
		int start, end;
		start = dst >> 3;
		end = (dst + len) >> 3;
		u8 *ptr = src;
		for (int j = start; j < end; j++, ptr+=8) {
			gbaEepromWrite8Bytes(ptr, j, eeprom_long);
		}
		*/
		break;
		}
	case SAVE_GBA_SRAM_32: {
		// SRAM: blind write
		u32 start = 0x0a000000 + dst;
		u8 *tmpdst = (u8*)start;
		sysSetBusOwners(true, true);
		for (u32 i = 0; i < len; i++, tmpdst++, src++) {
			*tmpdst = *src;
			swiDelay(10); // mabe we don't need this, but better safe than sorry
		}
		break;
		}
	case SAVE_GBA_FLASH_64: {
		bool atmel = gbaIsAtmel();
		if (atmel) {
			// only 64k, no bank switching required
			u32 len7 = len >> 7;
			u8 *tmpdst = (u8*)(0x0a000000+dst);
			for (u32 j = 0; j < len7; j++) {
				u32 ime = enterCriticalSection();
				*(vu8*)0x0a005555 = 0xaa;
				swiDelay(10);
				*(vu8*)0x0a002aaa = 0x55;
				swiDelay(10);
				*(vu8*)0x0a005555 = 0xa0;
				swiDelay(10);
				for (int i = 0; i < 128; i++) {
					*tmpdst = *src;
					swiDelay(10);
				}
				leaveCriticalSection(ime);
				while (*tmpdst != *src) {swiDelay(10);}
			}
			break;
		}
		nbanks = 1;
		}
	case SAVE_GBA_FLASH_128:
		// FLASH - must be opend by register magic, erased and then rewritten
		// FIXME: currently, you can only write "all or nothing"
		nbanks = 2;
		for (int j = 0; j < nbanks; j++) {
			*(vu8*)0x0a005555 = 0xaa;
			swiDelay(10);
			*(vu8*)0x0a002aaa = 0x55;
			swiDelay(10);
			*(vu8*)0x0a005555 = 0xb0;
			swiDelay(10);
			*(vu8*)0x0a000000 = (u8)j;
			swiDelay(10);
			//
			u32 start, sublen;
			if (j == 0) {
				start = 0x0a000000 + dst;
				sublen = (dst < 0x10000) ? min(len, (1 << 16) - dst) : 0;
			} else if (j == 1) {
				start = max(0x09ff0000 + dst, 0x0a000000);
				sublen = (dst + len < 0x10000) ? 0 : min(len, len - (0x10000 - dst));
			}
			u8 *tmpdst = (u8*)start;
			sysSetBusOwners(true, true);
			for (u32 i = 0; i < sublen; i++, tmpdst++, src++) {
				// we need to wait a few cycles before the hardware reacts!
				*(vu8*)0x0a005555 = 0xaa;
				swiDelay(10);
				*(vu8*)0x0a002aaa = 0x55;
				swiDelay(10);
				*(vu8*)0x0a005555 = 0xa0; // write byte command
				swiDelay(10);
				//
				*tmpdst = *src;
				swiDelay(10);
				//
				while (*tmpdst != *src) {swiDelay(10);}
			}
		}
		break;
	case SAVE_GBA_NONE:
		break;
	}
	return true;
}

bool gbaFormatSave(saveTypeGBA type)
{
	switch (type) {
		case SAVE_GBA_EEPROM_05:
		case SAVE_GBA_EEPROM_8:
			// TODO: eeprom is not supported yet
			break;
		case SAVE_GBA_SRAM_32:
			{
				// memset(data, 0, 1 << 15);
				u8 *data = new u8[1 << 15]();
				gbaWriteSave(0, data, 1 << 15, SAVE_GBA_SRAM_32);
				delete[] data;
			}
			break;
		case SAVE_GBA_FLASH_64:
		case SAVE_GBA_FLASH_128:
			*(vu8*)0x0a005555 = 0xaa;
			swiDelay(10);
			*(vu8*)0x0a002aaa = 0x55;
			swiDelay(10);
			*(vu8*)0x0a005555 = 0x80; // erase command
			swiDelay(10);
			*(vu8*)0x0a005555 = 0xaa;
			swiDelay(10);
			*(vu8*)0x0a002aaa = 0x55;
			swiDelay(10);
			*(vu8*)0x0a005555 = 0x10; // erase entire chip
			swiDelay(10);
			while (*(u8*)0x0a000000 != 0xff)
				swiDelay(10);
			break;
		case SAVE_GBA_NONE:
			break;
	}
	return true;
}
