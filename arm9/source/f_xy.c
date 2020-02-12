#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "types.h"
#include "utils.h"

//#define DEBUG

// flip each word and return as a u64 array
void aes_flip_to_64(u32 *in, u64* out)
{
    u32 endian_flip[4];
    u32 i;
    
    for(i = 0; i < 4; i++)
        endian_flip[i] = getbe32((u8*)&in[i]);

    out[0] = (u64)endian_flip[1] | ((u64)endian_flip[0] << 32);
    out[1] = (u64)endian_flip[3] | ((u64)endian_flip[2] << 32);
}

void aes_unflip_to_32(u64* in, u32* out)
{
    out[0] = getbe32((u8*)&in[0]+4);
    out[1] = getbe32((u8*)&in[0]);
    out[2] = getbe32((u8*)&in[1]+4);
    out[3] = getbe32((u8*)&in[1]);
}

void n128_lrot_3ds_internal(u32 *num, u32 shift)
{
	u64 tmp[2];
    u64 num_work[2];
    
    aes_flip_to_64(num, num_work);
    
	tmp[0] = num_work[0]<<shift;
	tmp[1] = num_work[1]<<shift;
	tmp[0] |= num_work[1]>>(64-shift);
	tmp[1] |= num_work[0]>>(64-shift);

    aes_unflip_to_32(tmp, num);
}

void n128_rrot_3ds_internal(u32 *num, u32 shift)
{
	u64 tmp[2];
    u64 num_work[2];

    aes_flip_to_64(num, num_work);
    
	tmp[0] = num_work[0]>>shift;
	tmp[1] = num_work[1]>>shift;
	tmp[0] |= (num_work[1]<<(64-shift));
	tmp[1] |= (num_work[0]<<(64-shift));

    aes_unflip_to_32(tmp, num);
}

void n128_lrot_3ds(u32 *num, u32 shift)
{
    u32 shift_cycle;
    while(shift > 0)
    {
        if(shift >= 32)
        {
            shift_cycle = 32;
            shift -= 32;
        }
        else
        {
            shift_cycle = shift;
            shift = 0;
        }
        n128_lrot_3ds_internal(num, shift_cycle);
    }
    
}

void n128_rrot_3ds(u32 *num, u32 shift)
{
    u32 shift_cycle;
    while(shift > 0)
    {
        if(shift >= 32)
        {
            shift_cycle = 32;
            shift -= 32;
        }
        else
        {
            shift_cycle = shift;
            shift = 0;
        }
        n128_rrot_3ds_internal(num, shift_cycle);
    }
    
}

void n128_add_3ds(u32 *a, u32 *b)
{
    u64 a64[4];
    u64 b64[4];
    aes_flip_to_64(a, a64);
    aes_flip_to_64(b, b64);
    
	uint64_t tmp = (a64[0]>>1)+(b64[0]>>1) + (a64[0] & b64[0] & 1);
	 
	tmp = tmp >> 63;
        a64[0] = a64[0] + b64[0];
        a64[1] = a64[1] + b64[1] + tmp;
    aes_unflip_to_32(a64, a);
}

void n128_lrot(uint64_t *num, uint32_t shift)
{
	uint64_t tmp[2];

	tmp[0] = num[0]<<shift;
	tmp[1] = num[1]<<shift;
	tmp[0] |= (num[1]>>(64-shift));
	tmp[1] |= (num[0]>>(64-shift));

	num[0] = tmp[0];
	num[1] = tmp[1];
}

void n128_rrot(uint64_t *num, uint32_t shift)
{
	uint64_t tmp[2];

	tmp[0] = num[0]>>shift;
	tmp[1] = num[1]>>shift;
	tmp[0] |= (num[1]<<(64-shift));
	tmp[1] |= (num[0]<<(64-shift));

	num[0] = tmp[0];
	num[1] = tmp[1];
}

void n128_add(uint64_t *a, uint64_t *b)
{
	uint64_t *a64 = a;
	uint64_t *b64 = b;
	uint64_t tmp = (a64[0]>>1)+(b64[0]>>1) + (a64[0] & b64[0] & 1);
	 
	tmp = tmp >> 63;
        a64[0] = a64[0] + b64[0];
        a64[1] = a64[1] + b64[1] + tmp;
}

void n128_sub(uint64_t *a, uint64_t *b)
{
	uint64_t *a64 = a;
	uint64_t *b64 = b;
	uint64_t tmp = (a64[0]>>1)-(b64[0]>>1) - ((a64[0]>>63) & (b64[0]>>63) & 1);
        
	tmp = tmp >> 63;
        a64[0] = a64[0] - b64[0];
        a64[1] = a64[1] - b64[1] - tmp;
}

void F_XY(uint32_t *key, uint32_t *key_x, uint32_t *key_y)
{
	int i;
	unsigned char key_xy[16];

	memset(key_xy, 0, 16);
	memset(key, 0, 16);
	for(i=0; i<16; i++)key_xy[i] = ((unsigned char*)key_x)[i] ^ ((unsigned char*)key_y)[i];

	key[0] = 0x1a4f3e79;
	key[1] = 0x2a680f5f;
	key[2] = 0x29590258;
	key[3] = 0xfffefb4e;

	n128_add((uint64_t*)key, (uint64_t*)key_xy);
	n128_lrot((uint64_t*)key, 42);
}

//F_XY_reverse does the reverse of F(X^Y): takes (normal)key, and does F in reverse to generate the original X^Y key_xy.
void F_XY_reverse(uint32_t *key, uint32_t *key_xy)
{
	uint32_t tmpkey[4];
	memset(key_xy, 0, 16);
	memset(tmpkey, 0, 16);
	memcpy(tmpkey, key, 16);

	key_xy[0] = 0x1a4f3e79;
	key_xy[1] = 0x2a680f5f;
	key_xy[2] = 0x29590258;
	key_xy[3] = 0xfffefb4e;

	n128_rrot((uint64_t*)tmpkey, 42);
	n128_sub((uint64_t*)tmpkey, (uint64_t*)key_xy);
	memcpy(key_xy, tmpkey, 16);
}

