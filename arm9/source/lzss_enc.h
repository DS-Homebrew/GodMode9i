#ifndef LZ77_COMPRESS_H
#define LZ77_COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#define LZS_WRAM      0x00       // VRAM not compatible (LZS_WRAM | LZS_NORMAL)
#define LZS_VRAM      0x01       // VRAM compatible (LZS_VRAM | LZS_NORMAL)
#define LZS_WFAST     0x80       // LZS_WRAM fast (LZS_WRAM | LZS_FAST)
#define LZS_VFAST     0x81       // LZS_VRAM fast (LZS_VRAM | LZS_FAST)
#define LZS_WBEST     0x40       // LZS_WRAM best (LZS_WRAM | LZS_BEST)
#define LZS_VBEST     0x41       // LZS_VRAM best (LZS_VRAM | LZS_BEST)

// Returned buffer must be freed manually
// pak_len will be the length of the compressed output
unsigned char *LZS_Encode(unsigned char *raw_buffer, int raw_len, int mode, int *pak_len);

#ifdef __cplusplus
}
#endif
#endif /* LZ77_COMPRESS_H */
