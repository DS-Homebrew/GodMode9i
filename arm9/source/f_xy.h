#ifndef _H_F_XY
#define _H_F_XY

#ifdef __cplusplus
extern "C" {
#endif

void F_XY(uint32_t *key, uint32_t *key_x, uint32_t *key_y);
void F_XY_reverse(uint32_t *key, uint32_t *key_xy);

void n128_lrot_3ds(u32 *num, u32 shift);
void n128_rrot_3ds(u32 *num, u32 shift);
void n128_add_3ds(u32 *a, u32 *b);

#ifdef __cplusplus
}
#endif

#endif

