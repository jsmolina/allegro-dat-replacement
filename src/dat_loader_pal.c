/* src/dat_loader_pal.c
 *
 * Carga una paleta desde disco y la convierte al formato interno de
 * Allegro 4: array de 256*3 bytes con componentes R,G,B en rango 0..63.
 *
 * Formatos soportados (detectados automaticamente por contenido):
 *
 *  1. Adobe ACT  - 768 bytes exactos (256 * RGB 0-255)
 *                  o 772 bytes (idem + u16 num_colors BE + u16 transparent BE)
 *  2. RIFF PAL   - cabecera 'RIFF'...'PAL '...'data'  (Windows palette)
 *  3. JASC PAL   - texto con cabecera "JASC-PAL"  (Paint Shop Pro)
 *  4. Raw 768    - fallback: fichero de exactamente 768 bytes tratado como
 *                  256*RGB 0-255 sin cabecera
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat_loader_pal.h"

static u8 to63(unsigned int v) {
    return (u8)((v > 255 ? 255 : v) / 4);
}

static u16 read_u16be_buf(const u8 *p) {
    return (u16)(((u16)p[0]<<8) | p[1]);
}

int load_act_to_pal63(const char *filename, u8 **out_pal)
{
    FILE *f;
    long  filesz;
    u8   *raw;
    u8   *pal;
    int   num_colors;
    int   i;

    *out_pal = NULL;

    f = fopen(filename, "rb");
    if (!f) { return 0; }

    fseek(f, 0, SEEK_END);
    filesz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (filesz <= 0 || filesz > 1024 * 1024) { fclose(f); return 0; }

    raw = (u8*)malloc((size_t)filesz);
    if (!raw) { fclose(f); return 0; }
    if ((long)fread(raw, 1, (size_t)filesz, f) != filesz) {
        free(raw); fclose(f); return 0;
    }
    fclose(f);

    pal = (u8*)calloc(256 * 3, 1);
    if (!pal) { free(raw); return 0; }

    /* 1. RIFF PAL */
    if (filesz >= 20 && memcmp(raw, "RIFF", 4) == 0 &&
        memcmp(raw + 8, "PAL ", 4) == 0)
    {
        u32 pos = 12;
        if (pos + 8 > (u32)filesz || memcmp(raw + pos, "data", 4) != 0) {
            free(pal); free(raw); return 0;
        }
        pos += 8; /* skip "data" + u32le data_size */
        /* u16le version, u16le count */
        if (pos + 4 > (u32)filesz) { free(pal); free(raw); return 0; }
        num_colors = (int)(raw[pos+2] | ((u16)raw[pos+3] << 8));
        pos += 4;
        if (num_colors <= 0 || num_colors > 256) num_colors = 256;
        for (i = 0; i < num_colors && pos + 4 <= (u32)filesz; i++, pos += 4) {
            pal[i*3+0] = to63(raw[pos+0]);
            pal[i*3+1] = to63(raw[pos+1]);
            pal[i*3+2] = to63(raw[pos+2]);
        }
        free(raw); *out_pal = pal; return 1;
    }

    /* 2. JASC PAL */
    if (filesz >= 8 && memcmp(raw, "JASC-PAL", 8) == 0)
    {
        char *p = (char*)raw;
        char *end = p + filesz;
        int nl = 0;
        /* skip 3 header lines */
        while (p < end && nl < 3) { if (*p++ == '\n') nl++; }
        num_colors = 0;
        while (p < end && num_colors < 256) {
            int r, g, b;
            while (p < end && (*p == '\r' || *p == '\n' || *p == ' ')) p++;
            if (p >= end) break;
            r = 0; while (p < end && *p >= '0' && *p <= '9') r = r*10 + (*p++ - '0');
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            g = 0; while (p < end && *p >= '0' && *p <= '9') g = g*10 + (*p++ - '0');
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            b = 0; while (p < end && *p >= '0' && *p <= '9') b = b*10 + (*p++ - '0');
            while (p < end && *p != '\n') p++;
            pal[num_colors*3+0] = to63((unsigned int)r);
            pal[num_colors*3+1] = to63((unsigned int)g);
            pal[num_colors*3+2] = to63((unsigned int)b);
            num_colors++;
        }
        free(raw); *out_pal = pal; return 1;
    }

    /* 3. Adobe ACT: 768 o 772 bytes */
    if (filesz == 768 || filesz == 772)
    {
        num_colors = 256;
        if (filesz == 772) {
            int nc = (int)read_u16be_buf(raw + 768);
            if (nc > 0 && nc <= 256) num_colors = nc;
        }
        for (i = 0; i < num_colors; i++) {
            pal[i*3+0] = to63(raw[i*3+0]);
            pal[i*3+1] = to63(raw[i*3+1]);
            pal[i*3+2] = to63(raw[i*3+2]);
        }
        free(raw); *out_pal = pal; return 1;
    }

    /* 4. Raw fallback: multiplo de 3, <= 768 bytes */
    if (filesz % 3 == 0 && filesz >= 3 && filesz <= 768)
    {
        num_colors = (int)(filesz / 3);
        for (i = 0; i < num_colors; i++) {
            pal[i*3+0] = to63(raw[i*3+0]);
            pal[i*3+1] = to63(raw[i*3+1]);
            pal[i*3+2] = to63(raw[i*3+2]);
        }
        free(raw); *out_pal = pal; return 1;
    }

    fprintf(stderr, "Error: formato de paleta no reconocido en '%s'\n"
                    "       Formatos soportados: ACT (768/772 bytes), RIFF PAL, JASC PAL, raw RGB 768 bytes\n",
            filename);
    free(pal); free(raw); return 0;
}

/* ------------------------------------------------------------------ */
/* load_bmp_to_pal63                                                    */
/* Extrae la tabla de colores de un BMP indexado (1/4/8 bpp).          */
/* ------------------------------------------------------------------ */
int load_bmp_to_pal63(const char *filename, u8 **out_pal)
{
    FILE          *f;
    unsigned char  fh[14];  /* BITMAPFILEHEADER */
    unsigned char  ih[40];  /* BITMAPINFOHEADER */
    u32            biBitCount;
    u32            biClrUsed;
    u32            num_colors;
    u32            ct_offset;
    u32            i;
    u8            *pal;

    *out_pal = NULL;

    f = fopen(filename, "rb");
    if (!f) return 0;

    /* BITMAPFILEHEADER: 2 bytes signature + 12 bytes */
    if (fread(fh, 1, 14, f) != 14 || fh[0] != 'B' || fh[1] != 'M') {
        fclose(f); return 0;
    }

    /* BITMAPINFOHEADER: 40 bytes */
    if (fread(ih, 1, 40, f) != 40) { fclose(f); return 0; }

    biBitCount = (u32)ih[14] | ((u32)ih[15]<<8);
    biClrUsed  = (u32)ih[32] | ((u32)ih[33]<<8) | ((u32)ih[34]<<16) | ((u32)ih[35]<<24);

    /* Solo BMPs indexados tienen tabla de colores */
    if (biBitCount != 1 && biBitCount != 4 && biBitCount != 8) {
        fprintf(stderr, "Error: '%s' es un BMP de %u bpp y no tiene tabla de colores.\n"
                        "       Usa un BMP indexado (1, 4 u 8 bpp).\n",
                filename, biBitCount);
        fclose(f); return 0;
    }

    /* Numero de entradas en la tabla de colores */
    if (biClrUsed > 0 && biClrUsed <= 256)
        num_colors = biClrUsed;
    else
        num_colors = 1u << biBitCount;  /* 2, 16, o 256 */

    /* La tabla de colores sigue inmediatamente al BITMAPINFOHEADER (offset 54)
       a menos que haya un header extendido; en ese caso usamos bfOffBits - tabla_size. */
    ct_offset = 14 + 40; /* 54 para headers estandar */
    /* Si el header BMP es mas grande de 40 bytes (BITMAPV4/V5), saltar el extra */
    {
        u32 ih_size = (u32)ih[0] | ((u32)ih[1]<<8) | ((u32)ih[2]<<16) | ((u32)ih[3]<<24);
        if (ih_size > 40) ct_offset = 14 + ih_size;
    }

    fseek(f, (long)ct_offset, SEEK_SET);

    pal = (u8*)calloc(256 * 3, 1);
    if (!pal) { fclose(f); return 0; }

    /* Leer RGBQUADs: { Blue, Green, Red, Reserved } */
    for (i = 0; i < num_colors; i++) {
        unsigned char quad[4];
        if (fread(quad, 1, 4, f) != 4) { free(pal); fclose(f); return 0; }
        pal[i*3+0] = to63(quad[2]);  /* Red   (quad[2]) */
        pal[i*3+1] = to63(quad[1]);  /* Green (quad[1]) */
        pal[i*3+2] = to63(quad[0]);  /* Blue  (quad[0]) */
    }

    fclose(f);
    *out_pal = pal;
    return 1;
}
