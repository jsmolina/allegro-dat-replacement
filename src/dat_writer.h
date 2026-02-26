// src/dat_writer.h
#ifndef DAT_WRITER_H
#define DAT_WRITER_H
#include <stdio.h>
#include "allegro_dat_structs.h"

int dat_write(const char *filename, AllegroDat *dat);

// size helpers (host-endian to logical byte counts)
static inline s32 dat_len_bmp(const DatBitmap *b){ return 2+2+2 + (s32)(b->width * b->height * (b->bits_per_pixel/8)); }
static inline s32 dat_len_pal(void){ return 256*3; }
static inline s32 dat_len_rle(const DatRleSprite *r){ return 2+2+2+4 + (s32)r->len_image; }
// font: 8 -> 2 + 95*8, 16 -> 2 + 95*16
static inline s32 dat_len_font(const DatFont *f){ if(f->font_size==8) return 2 + 95*8; else if(f->font_size==16) return 2 + 95*16; return 2; }

#endif
