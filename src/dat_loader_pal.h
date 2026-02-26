// src/dat_loader_pal.h
#ifndef DAT_LOADER_PAL_H
#define DAT_LOADER_PAL_H
#include "allegro_dat_structs.h"

// Loads a RIFF PAL (Adobe/Microsoft .act/.pal) and converts to Allegro 0-63 range.
// Returns 0 on error.
int load_act_to_pal63(const char *filename, u8 **out_pal);

#endif
