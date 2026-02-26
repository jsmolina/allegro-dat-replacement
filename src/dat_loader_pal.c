// src/dat_loader_pal.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat_loader_pal.h"

// Very small RIFF PAL reader (common ACT/PAL variant):
// 'RIFF' <u32 size> 'PAL ' 'data' <u32 size> <u16 ver=0x0300> <u16 n> { n * (r,g,b,flags) }

static int read_u32le(FILE *f, u32 *v){ unsigned char b[4]; if(fread(b,1,4,f)!=4) return 0; *v = (u32)b[0] | ((u32)b[1]<<8) | ((u32)b[2]<<16) | ((u32)b[3]<<24); return 1; }
static int read_u16le(FILE *f, u16 *v){ unsigned char b[2]; if(fread(b,1,2,f)!=2) return 0; *v = (u16)(b[0] | (b[1]<<8)); return 1; }

int load_act_to_pal63(const char *filename, u8 **out_pal)
{
    *out_pal = NULL;
    FILE *f = fopen(filename, "rb"); if(!f) return 0;
    char riff[4]; if(fread(riff,1,4,f)!=4){ fclose(f); return 0; }
    if(memcmp(riff,"RIFF",4)!=0){ fclose(f); return 0; }
    u32 riff_size; if(!read_u32le(f,&riff_size)){ fclose(f); return 0; }
    char palhdr[4]; if(fread(palhdr,1,4,f)!=4){ fclose(f); return 0; }
    if(memcmp(palhdr,"PAL ",4)!=0){ fclose(f); return 0; }
    char data_id[4]; if(fread(data_id,1,4,f)!=4){ fclose(f); return 0; }
    if(memcmp(data_id,"data",4)!=0){ fclose(f); return 0; }
    u32 data_size; if(!read_u32le(f,&data_size)){ fclose(f); return 0; }
    u16 ver, n; if(!read_u16le(f,&ver)){ fclose(f); return 0; } if(!read_u16le(f,&n)){ fclose(f); return 0; }
    if(ver!=0x0300 || n==0){ fclose(f); return 0; }
    u8 *pal = (u8*)malloc(256*3); if(!pal){ fclose(f); return 0; }
    memset(pal,0,256*3);
    for(u16 i=0;i<n;i++){
        unsigned char rgba[4]; if(fread(rgba,1,4,f)!=4){ free(pal); fclose(f); return 0; }
        // RGBA in 0..255. Convert to Allegro 0..63 (divide by 4, clamp).
        pal[i*3+0] = (u8)(rgba[0]/4); // R
        pal[i*3+1] = (u8)(rgba[1]/4); // G
        pal[i*3+2] = (u8)(rgba[2]/4); // B
    }
    fclose(f);
    *out_pal = pal; return 1;
}
