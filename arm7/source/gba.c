#include "gba.h"

#include <nds/dma.h>
#include <nds/memory.h>
#include <string.h>

#define EEPROM_ADDRESS (0x09FFFF00)
#define REG_EEPROM *(vu16 *)(EEPROM_ADDRESS)
#define DMA3_CR_H *(vu16 *)(0x040000DE)

void EEPROM_SendPacket(u16 *packet, int size)
{
	REG_EXMEMSTAT = (REG_EXMEMSTAT & 0xFFE3) | 0x000C;
	DMA3_SRC = (u32)packet;
	DMA3_DEST = EEPROM_ADDRESS;
	DMA3_CR = 0x80000000 + size;
	while((DMA3_CR_H & 0x8000) != 0);
}

void EEPROM_ReceivePacket(u16 *packet, int size)
{
	REG_EXMEMSTAT = (REG_EXMEMSTAT & 0xFFE3) | 0x000C;
	DMA3_SRC = EEPROM_ADDRESS;
	DMA3_DEST = (u32)packet;
	DMA3_CR = 0x80000000 + size;
	while((DMA3_CR_H & 0x8000) != 0);
}

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

void gbaEepromWrite8Bytes(u8 *in, u16 addr, bool short_addr)
{
	u16 packet_length = short_addr ? 73 : 81;
	u16 packet[packet_length];

	memset(packet, 0, packet_length * 2);

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

bool gbaReadEeprom(u8 *dst, u32 src, u32 len)
{
	if(!dst)
		return false;

	int start, end;
	start = src >> 3;
	end = (src + len) >> 3;
	u8 *ptr = dst;
	for (int j = start; j < end; j++) {
		gbaEepromRead8Bytes(ptr, j, len == 0x200);
		ptr += 8;
	}

	return true;
}

bool gbaWriteEeprom(u32 dst, u8 *src, u32 len)
{
	if(!src)
		return false;

	int start, end;
	start = dst >> 3;
	end = (dst + len) >> 3;
	u8 *ptr = src;
	for (int j = start; j < end; j++, ptr += 8) {
		gbaEepromWrite8Bytes(ptr, j, len == 0x200);
	}
	
	return true;
}
