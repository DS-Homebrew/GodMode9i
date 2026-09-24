#pragma once

#include <nds.h>
#include <nds/disc_io.h>

void nandio_set_fat_sig_fix(u32 offset);

extern const DISC_INTERFACE io_dsi_nand;

// Real DSi Console ID, derived during NAND mount (0x02F00000 holds AES key3 material, not the
// plain ID). Valid (nandConsoleIDValid != 0) only after a successful nandio_startup on a DSi.
extern u8 nandConsoleID[8];
extern u8 nandConsoleIDValid;
