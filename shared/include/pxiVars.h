#ifndef PXIVARS_H
#define PXIVARS_H

#ifdef __cplusplus
extern "C" {
#endif

#define PXI_MAIN PxiChannel_User0

typedef enum {
	PXI_FAILURE,
	PXI_SUCCESS,
	GET_ARM7_VARS,
	GBA_READ_EEPROM,
	GBA_WRITE_EEPROM,
} PxiCommand;

typedef enum {
	CAPTURE_MODE_PREVIEW = 1, // 256x192
	CAPTURE_MODE_CAPTURE = 2  // 640x480
} CaptureMode;

#ifdef __cplusplus
}
#endif

#endif // PXIVARS_H
