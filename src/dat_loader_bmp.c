// src/dat_loader_bmp.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat_loader_bmp.h"

#pragma pack(push,1)
typedef struct { u16 bfType; u32 bfSize; u16 bfReserved1; u16 bfReserved2; u32 bfOffBits; } BMPFILEHDR;
typedef struct { u32 biSize; s32 biWidth; s32 biHeight; u16 biPlanes; u16 biBitCount; u32 biCompression; u32 biSizeImage; s32 biXPelsPerMeter; s32 biYPelsPerMeter; u32 biClrUsed; u32 biClrImportant; } BMPINFOHDR;
#pragma pack(pop)

static size_t row_stride(int width, int bpp){ size_t raw = (size_t)width * (size_t)(bpp/8); size_t pad = (4 - (raw % 4)) & 3; return raw + pad; }

int load_bmp_to_dat_bitmap(const char *filename, DatBitmap **out)
{
    *out = NULL;
    FILE *f = fopen(filename, "rb"); if(!f) return 0;
    BMPFILEHDR fh; BMPINFOHDR ih; if(fread(&fh, sizeof(fh),1,f)!=1){ fclose(f); return 0;} if(fread(&ih,sizeof(ih),1,f)!=1){ fclose(f); return 0;}
    if(fh.bfType != 0x4D42){ fclose(f); return 0; }
    if(ih.biCompression != 0){ fclose(f); return 0; }
    if(!(ih.biBitCount==8 || ih.biBitCount==24 || ih.biBitCount==32)){ fclose(f); return 0; }
    int w = ih.biWidth; int h = ih.biHeight; int H = (h<0)?-h:h; int bottom_up = (h>0);
    size_t stride = row_stride(w, ih.biBitCount);
    // Skip palette if present on 8-bit BMPs
    fseek(f, fh.bfOffBits, SEEK_SET);
    size_t datasz = stride * H; u8 *pix = (u8*)malloc(datasz); if(!pix){ fclose(f); return 0; }
    if(fread(pix, 1, datasz, f)!=datasz){ free(pix); fclose(f); return 0; }
    fclose(f);

    // Flatten to DAT layout (top-down, tight)
    int bppB = ih.biBitCount/8; size_t row_out = (size_t)w * bppB; size_t total = (size_t)H * row_out; u8 *flat = (u8*)malloc(total); if(!flat){ free(pix); return 0; }
    for(int y=0;y<H;y++){
        const u8 *src = bottom_up ? pix + (size_t)(H-1-y)*stride : pix + (size_t)y*stride; 
        memcpy(flat + (size_t)y*row_out, src, row_out);
    }
    free(pix);

    DatBitmap *db = (DatBitmap*)calloc(1,sizeof(DatBitmap)); if(!db){ free(flat); return 0; }
    db->bits_per_pixel = (s16)ih.biBitCount; db->width=(u16)w; db->height=(u16)H; db->image=flat;
    *out = db; return 1;
}
