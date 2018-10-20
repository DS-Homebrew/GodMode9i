#include <nds.h>
#include <stdlib.h>
#include <stdio.h>

#include <fat.h>

#include "screenshot.h"
#include "bmp.h"

void wait();

void screenshot(u8* buffer) {

	u8 vram_cr_temp=VRAM_A_CR;
	VRAM_A_CR=VRAM_A_LCD;

	u8* vram_temp=(u8*)malloc(128*1024);
	dmaCopy(VRAM_A, vram_temp, 128*1024);

	REG_DISPCAPCNT=DCAP_BANK(0)|DCAP_ENABLE|DCAP_SIZE(3);
	while(REG_DISPCAPCNT & DCAP_ENABLE);

	dmaCopy(VRAM_A, buffer, 256*192*2);
	dmaCopy(vram_temp, VRAM_A, 128*1024);
	
	VRAM_A_CR=vram_cr_temp;
	
	free(vram_temp);

}

void screenshot(char* filename) {

	//fatInitDefault();
	FILE* file=fopen(filename, "w");

	u8* temp=(u8*)malloc(256*192*2);
	dmaCopy(VRAM_B, temp, 256*192*2);
	fwrite(temp, 1, 256*192*2, file);
	fclose(file);
	free(temp);
}

void write16(u16* address, u16 value) {

	u8* first=(u8*)address;
	u8* second=first+1;

	*first=value&0xff;
	*second=value>>8;
}

void write32(u32* address, u32 value) {

	u8* first=(u8*)address;
	u8* second=first+1;
	u8* third=first+2;
	u8* fourth=first+3;

	*first=value&0xff;
	*second=(value>>8)&0xff;
	*third=(value>>16)&0xff;
	*fourth=(value>>24)&0xff;
}

void screenshotbmp(const char* filename) {

	//fatInitDefault();
	FILE* file=fopen(filename, "wb");

	REG_DISPCAPCNT=DCAP_BANK(3)|DCAP_ENABLE|DCAP_SIZE(3);
	while(REG_DISPCAPCNT & DCAP_ENABLE);

	u8* temp=(u8*)malloc(256*192*3+sizeof(INFOHEADER)+sizeof(HEADER));

	HEADER* header=(HEADER*)temp;
	INFOHEADER* infoheader=(INFOHEADER*)(temp+sizeof(HEADER));

	write16(&header->type, 0x4D42);
	write32(&header->size, 256*192*3+sizeof(INFOHEADER)+sizeof(HEADER));
	write32(&header->offset, sizeof(INFOHEADER)+sizeof(HEADER));
	write16(&header->reserved1, 0);
	write16(&header->reserved2, 0);

	write16(&infoheader->bits, 24);
	write32(&infoheader->size, sizeof(INFOHEADER));
	write32(&infoheader->compression, 0);
	write32(&infoheader->width, 256);
	write32(&infoheader->height, 192);
	write16(&infoheader->planes, 1);
	write32(&infoheader->imagesize, 256*192*3);
	write32(&infoheader->xresolution, 0);
	write32(&infoheader->yresolution, 0);
	write32(&infoheader->importantcolours, 0);
	write32(&infoheader->ncolours, 0);

	for(int y=0;y<192;y++)
	{
		for(int x=0;x<256;x++)
		{
			u16 color=VRAM_D[256*191-y*256+x];

			u8 b=(color&31)<<3;
			u8 g=((color>>5)&31)<<3;
			u8 r=((color>>10)&31)<<3;

			temp[((y*256)+x)*3+sizeof(INFOHEADER)+sizeof(HEADER)]=r;
			temp[((y*256)+x)*3+1+sizeof(INFOHEADER)+sizeof(HEADER)]=g;
			temp[((y*256)+x)*3+2+sizeof(INFOHEADER)+sizeof(HEADER)]=b;
		}
	}

	DC_FlushAll();
	fwrite(temp, 1, 256*192*3+sizeof(INFOHEADER)+sizeof(HEADER), file);
	fclose(file);
	free(temp);
}
