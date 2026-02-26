// src/dat_loader_font.h
#ifndef DAT_LOADER_FONT_H
#define DAT_LOADER_FONT_H
#include "allegro_dat_structs.h"

// Build FONT 8x8 or 8x16 from a BMP sprite sheet (95 ASCII chars from 32..126) laid horizontally.
// The BMP must be exactly width = 95*8, height = 8 or 16, truecolor or 8-bit.
// Threshold (0..255) is used for binarization if truecolor.
// Returns 0 on error.
int build_font8_from_bmp(const char *filename, int threshold, DatFont **out);
int build_font16_from_bmp(const char *filename, int threshold, DatFont **out);

#endif
