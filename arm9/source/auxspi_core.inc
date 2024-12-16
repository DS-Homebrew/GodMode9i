/*
 * savegame_manager: a tool to backup and restore savegames from Nintendo
 *  DS cartridges. Nintendo DS and all derivative names are trademarks
 *  by Nintendo. EZFlash 3-in-1 is a trademark by EZFlash.
 *
 * auxspi_core.inc: A thin reimplementation of the AUXSPI protocol
 *   (low level functions)
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



inline void auxspi_wait_busy()
{
	while (REG_AUXSPICNT & 0x80);
}

inline void auxspi_wait_wip()
{
	do { REG_AUXSPIDATA = 0; auxspi_wait_busy(); } while (REG_AUXSPIDATA & 0x01);	// WIP (Write In Progress) ?
}

inline void auxspi_open(u8 device)
{
	REG_AUXSPICNT = 0xa040 | (device & 3);
	auxspi_wait_busy();
}

inline void auxspi_close()
{
	REG_AUXSPIDATA = 0;
	auxspi_wait_busy();
	REG_AUXSPICNT = 0;
	auxspi_wait_busy();
}

inline void auxspi_close_lite()
{
	REG_AUXSPICNT = 0x40;
	auxspi_wait_busy();
}

inline u8 auxspi_transfer(u8 out)
{
	REG_AUXSPIDATA = out;
	auxspi_wait_busy();
	return REG_AUXSPIDATA;
}

inline void auxspi_write(u8 out)
{
	auxspi_transfer(out);
}

inline u8 auxspi_read()
{
	return auxspi_transfer(0);
}

inline u16 auxspi_read_16()
{
	REG_AUXSPIDATA = 0;
	auxspi_wait_busy();
	return REG_AUXSPIDATA;
}

inline void auxspi_disable_infrared_core()
{
	auxspi_open(0);
	swiDelay(1200);
	auxspi_open(2);
	auxspi_write(0);
	swiDelay(1200);
}
