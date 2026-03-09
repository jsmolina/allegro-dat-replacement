/* src/dat_loader_pal.h */
#ifndef DAT_LOADER_PAL_H
#define DAT_LOADER_PAL_H
#include "allegro_dat_structs.h"

/* Carga una paleta desde multiples formatos (ACT 768/772 bytes, RIFF PAL,
   JASC PAL, raw RGB). Devuelve array de 256*3 bytes en rango 0-63.
   Caller debe free(). */
int load_act_to_pal63(const char *filename, u8 **out_pal);

/* Extrae la tabla de colores de un BMP indexado (1/4/8 bpp) como paleta Allegro.
   Devuelve array de 256*3 bytes en rango 0-63. Caller debe free().
   Devuelve 0 si el BMP es de 24/32 bpp (no tiene tabla de colores). */
int load_bmp_to_pal63(const char *filename, u8 **out_pal);

#endif
