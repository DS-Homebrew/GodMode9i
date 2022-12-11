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

inline u32 min(u32 i, u32 j) { return (i < j) ? i : j;}
inline u32 max(u32 i, u32 j) { return (i > j) ? i : j;}

// -----------------------------------------------------
#define MAGIC_EEPR 0x52504545
#define MAGIC_SRAM 0x4d415253
#define MAGIC_FLAS 0x53414c46

#define MAGIC_H1M_ 0x5f4d3148

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

void readEeprom(u8 *dst, u32 src, u32 len) {
	// EEPROM reading needs to happen on ARM7
	sysSetCartOwner(BUS_OWNER_ARM7);
	fifoSendValue32(FIFO_USER_01, 0x44414552 /* 'READ' */);
	fifoSendAddress(FIFO_USER_01, dst);
	fifoSendValue32(FIFO_USER_01, src);
	fifoSendValue32(FIFO_USER_01, len);

	// Read the data from FIFO
	u8 *ptr = dst;
	while(ptr < dst + len) {
		if(fifoCheckDatamsg(FIFO_USER_02)) {
			fifoGetDatamsg(FIFO_USER_02, 8, ptr);
			ptr += 8;
		}
	}

	sysSetCartOwner(BUS_OWNER_ARM9);
}

void writeEeprom(u32 dst, u8 *src, u32 len) {
	// EEPROM writing needs to happen on ARM7
	sysSetCartOwner(BUS_OWNER_ARM7);
	fifoSendValue32(FIFO_USER_01, 0x54495257 /* 'WRIT' */);
	fifoSendValue32(FIFO_USER_01, dst);
	fifoSendAddress(FIFO_USER_01, src);
	fifoSendValue32(FIFO_USER_01, len);

	// Wait for it to finish
	fifoWaitValue32(FIFO_USER_02);
	fifoGetValue32(FIFO_USER_02);

	sysSetCartOwner(BUS_OWNER_ARM9);
}

saveTypeGBA gbaGetSaveType() {
	// Search for any one of the magic version strings in the ROM. They are always dword-aligned.
	uint32 *data = (uint32*)0x08000000;
	
	for (int i = 0; i < (0x02000000 >> 2); i++, data++) {
		if (*data == MAGIC_EEPR) {
			u8 *buffer = new u8[0x2000];
			readEeprom(buffer, 0, 0x2000);

			// Check if first 0x800 bytes are duplicates of the first 8
			for(int j = 8; j < 0x800; j += 8) {
				if(memcmp(buffer, buffer + j, 8) != 0) {
					delete[] buffer;
					return SAVE_GBA_EEPROM_8;
				}
			}
			delete[] buffer;
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
	
	switch (type) {
	case SAVE_GBA_EEPROM_05:
	case SAVE_GBA_EEPROM_8: {
		readEeprom(dst, src, len);
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

u16 gbaGetFlashId()
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

	return dev << 8 | man;
}

bool gbaIsAtmel()
{
	return gbaGetFlashId() == 0x3d1f;
}

bool gbaWriteSave(u32 dst, u8 *src, u32 len, saveTypeGBA type)
{
	int nbanks = 2; // for type 4,5
	
	switch (type) {
	case SAVE_GBA_EEPROM_05:
	case SAVE_GBA_EEPROM_8: {
		writeEeprom(dst, src, len);
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
			// EEPROM doesn't need erasing
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

#define GPIO_DAT (*(vu16*) 0x080000c4)
#define GPIO_DIR (*(vu16*) 0x080000c6)
#define GPIO_CNT (*(vu16*) 0x080000c8)

#define RTC_CMD_READ(x) (((x)<<1) | 0x61)
#define RTC_CMD_WRITE(x) (((x)<<1) | 0x60)

static void rtcEnable()
{
	GPIO_CNT = 1;
}

static void rtcDisable()
{
	GPIO_CNT = 0;
}

static void rtcWriteCmd(u8 cmd)
{
	int l;
	u16 b;
	u16 v = cmd <<1;
	for(l=7; l>=0; l--)
	{
		b = (v>>l) & 0x2;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 5;
	}
}

static void rtcWriteData(u8 data)
{
	int l;
	u16 b;
	u16 v = data <<1;
	for(l=0; l<8; l++)
	{
		b = (v>>l) & 0x2;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 4;
		GPIO_DAT = b | 5;
	}
}
static u8 rtcReadData()
{
	int j,l;
	u16 b;
	int v = 0;
	for(l=0; l<8; l++)
	{
		for(j=0;j<5; j++)
			GPIO_DAT = 4;
		GPIO_DAT = 5;
		b = GPIO_DAT;
		v = v | ((b & 2)<<l);
	}
	v = v>>1;
	return v;
}

bool gbaGetRtc(u8 *rtc)
{
	rtcEnable();
	
	int i;
	GPIO_DAT = 1;
	GPIO_DIR = 7;
	GPIO_DAT = 1;
	GPIO_DAT = 5;
	rtcWriteCmd(RTC_CMD_READ(2));
	GPIO_DIR = 5;
	for(i=0; i<4; i++)
		rtc[i] = rtcReadData();
	GPIO_DIR = 5;
	for(i=4; i<7; i++)
		rtc[i] = rtcReadData();
	return 0;
	
	rtcDisable();
}
