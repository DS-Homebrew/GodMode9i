#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t    s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;


enum flags
{
	ExtractFlag = (1<<0),
	InfoFlag = (1<<1),
	PlainFlag = (1<<2),
	VerboseFlag = (1<<3),
	VerifyFlag = (1<<4),
	RawFlag = (1<<5),
	ShowKeysFlag = (1<<6),
	DecompressCodeFlag = (1<<7)
};

enum validstate
{
	Unchecked = 0,
	Good = 1,
	Fail = 2,
};

enum sizeunits
{
	sizeKB = 0x400,
	sizeMB = 0x100000,
};

#endif
