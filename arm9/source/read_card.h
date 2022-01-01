/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef READ_CARD_H
#define READ_CARD_H

#include <nds/ndstypes.h>
#include <nds/memory.h>
#include <stdlib.h>

#include "ndsheaderbanner.h"

#define CARD_NDS_HEADER_SIZE (0x200)
#define CARD_SECURE_AREA_OFFSET (0x4000)
#define CARD_SECURE_AREA_SIZE (0x4000)
#define CARD_DATA_OFFSET (0x8000)
#define CARD_DATA_BLOCK_SIZE (0x200)
#define MODC_AREA_SIZE          0x4000

#ifdef __cplusplus
extern "C" {
#endif

extern u32 cardNandRomEnd;
extern u32 cardNandRwStart;

int cardInit (sNDSHeaderExt* ndsHeader);

void cardRead (u32 src, void* dest, bool nandSave);

u32 cardGetId (void);

void cardWriteNand (void* src, u32 dest);

#ifdef __cplusplus
}
#endif

#endif // READ_CARD_H

