// src/dat_loader_bmp.h
#ifndef DAT_LOADER_BMP_H
#define DAT_LOADER_BMP_H
#include "allegro_dat_structs.h"

// Loads a Windows BMP (8/24/32 bpp, uncompressed) and converts to DAT bitmap layout (top-down, tight)
// Returns 0 on error
int load_bmp_to_dat_bitmap(const char *filename, DatBitmap **out);

#endif
