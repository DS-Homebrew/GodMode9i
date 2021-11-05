/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo.
 *
 * gba.h: header file for gba.cpp
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
#ifndef __GBA_H__
#define __GBA_H__

enum saveTypeGBA {
	SAVE_GBA_NONE = 0,
	SAVE_GBA_EEPROM_05, // 512 bytes
	SAVE_GBA_EEPROM_8, // 8k
	SAVE_GBA_SRAM_32, // 32k
	SAVE_GBA_FLASH_64, // 64k
	SAVE_GBA_FLASH_128 // 128k
};


// --------------------
bool gbaIsGame();
saveTypeGBA gbaGetSaveType();
uint32 gbaGetSaveSize(saveTypeGBA type = SAVE_GBA_NONE);
uint32 gbaGetSaveSizeLog2(saveTypeGBA type = SAVE_GBA_NONE);

bool gbaReadSave(u8 *dst, u32 src, u32 len, saveTypeGBA type);
bool gbaWriteSave(u32 dst, u8 *src, u32 len, saveTypeGBA type);
bool gbaFormatSave(saveTypeGBA type);


#endif // __GBA_H__