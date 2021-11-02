/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo. EZFlash 3-in-1 is a trademark by EZFlash.
 *
 * auxspi.h: Header for auxspi.cpp
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
/*
  This is a thin reimplementation of the AUXSPI protocol at low levels.
  It is used to implement various experimental procedures to test accessing the HG/SS save chip. */

#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <nds.h>

// This is a handy typedef for nonstandard SPI buses, i.e. games with some
//  extra hardware on the cartridge. The following values mean:
// AUXSPI_DEFAULT: A regular game with no exotic hardware.
// AUXSPI_INFRARED: A game with an infrared transceiver.
//  Games known to use this hardware:
//  - Personal Trainer: Walking (aka Laufrhytmus DS, Walk With Me, ...)
//  - Pokemon HeartGold/SoulSilver/Black/White
// AUXSPI_BLUETOOTH: A game with a Bluetooth transceiver. The only game using this
//  hardware is Pokemon Typing DS.
//
typedef enum {
	AUXSPI_DEFAULT,
	AUXSPI_INFRARED,
	AUXSPI_BLUETOOTH,
	AUXSPI_FLASH_CARD = 999
} auxspi_extra;

// These functions reimplement relevant parts of "card.cpp", in a way that is easier to modify.
uint8 auxspi_save_type(auxspi_extra extra = AUXSPI_DEFAULT);
uint32 auxspi_save_size(auxspi_extra extra = AUXSPI_DEFAULT);
uint8 auxspi_save_size_log_2(auxspi_extra extra = AUXSPI_DEFAULT);
uint32 auxspi_save_jedec_id(auxspi_extra extra = AUXSPI_DEFAULT);
uint8 auxspi_save_status_register(auxspi_extra extra = AUXSPI_DEFAULT);
void auxspi_read_data(uint32 addr, uint8* buf, uint32 cnt, uint8 type = 0,auxspi_extra extra = AUXSPI_DEFAULT);
void auxspi_write_data(uint32 addr, uint8 *buf, uint32 cnt, uint8 type = 0,auxspi_extra extra = AUXSPI_DEFAULT);
void auxspi_erase(auxspi_extra extra = AUXSPI_DEFAULT);
void auxspi_erase_sector(u32 sector, auxspi_extra extra = AUXSPI_DEFAULT);

// These functions are used to identify exotic hardware.
auxspi_extra auxspi_has_extra();
//bool auxspi_has_infrared();

void auxspi_disable_extra(auxspi_extra extra = AUXSPI_DEFAULT);
void auxspi_disable_infrared();
void auxspi_disable_big_protection();

// The following function returns true if this is a type 3 save (big saves, Flash memory), but
//  the JEDEC ID is not known.
bool auxspi_is_unknown_type3(auxspi_extra extra = AUXSPI_DEFAULT);

#endif