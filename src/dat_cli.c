/* src/dat_cli.c
   ANSI-C CLI for Allegro 4 DAT builder
   No Win32, no POSIX-specific APIs, pure ANSI C89/C90 compatible.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "allegro_dat_structs.h"
#include "dat_writer.h"
#include "dat_loader_bmp.h"
#include "dat_loader_pal.h"
#include "dat_loader_font.h"

/* === small helpers === */

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static void set_prop(Property *p, const char type4[4], const char *value)
{
    memcpy(p->magic, "prop", 4);
    memcpy(p->type,  type4, 4);
    p->len_body = (u32)strlen(value);
    p->body = dupstr(value);
}

/* Basename portable (no Win32 special cases) */
static const char *basename_portable(const char *path)
{
    const char *s = strrchr(path, '/');
    if (s) return s + 1;
    return path;
}

/* ANSI‑C portable date string */
static void now_datestr(char *buf, size_t n)
{
    time_t t = time(NULL);
    /* ANSI C: localtime() returns static storage, but valid */
    struct tm *tmv = localtime(&t);
    if (!tmv) {
        /* fallback */
        strncpy(buf, "unknown", n);
        buf[n - 1] = '\0';
        return;
    }
    /* No strftime? ANSI C *does* include strftime(), so OK */
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tmv);
}

/* === Usage === */

static void usage(void)
{
    printf("\nAllegro 4 DAT creator (ANSI C)\n\n");
    printf("Usage:\n");
    printf("  dat create out.dat\n");
    printf("      [--bmp file.bmp]*\n");
    printf("      [--pal file.act]*\n");
    printf("      [--rle file.rle]*\n");
    printf("      [--font8-bmp file.bmp]*\n");
    printf("      [--font16-bmp file.bmp]*\n\n");
}

/* === Main === */

int main(int argc, char **argv)
{
    int i;
    const char *out;
    char datebuf[64];

    if (argc < 3) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "create") != 0) {
        usage();
        return 1;
    }

    out = argv[2];
    now_datestr(datebuf, sizeof(datebuf));

    /* Allocate the main DAT root */
    {
        AllegroDat *dat = (AllegroDat*)calloc(1, sizeof(AllegroDat));
        DatObject  *objs;

        if (!dat) {
            fprintf(stderr, "Out of memory.\n");
            return 1;
        }

        dat->pack_magic  = 0x736C682Eu;  /* 'slh.' big endian */
        dat->dat_magic   = 0x414C4C2Eu;  /* 'ALL.' */
        dat->num_objects = 0;

        objs = (DatObject*)calloc(1024, sizeof(DatObject));
        if (!objs) {
            fprintf(stderr, "OOM objects.\n");
            free(dat);
            return 1;
        }
        dat->objects = objs;

        /* Parse all arguments */
        for (i = 3; i < argc; i++) {

            /* BMP */
            if (strcmp(argv[i], "--bmp") == 0 && i + 1 < argc) {
                DatBitmap *bmp = NULL;
                if (!load_bmp_to_dat_bitmap(argv[i + 1], &bmp)) {
                    fprintf(stderr, "ERR: cannot load BMP %s\n", argv[i + 1]);
                    free_allegro_dat(dat);
                    return 2;
                }

                {
                    DatObject *o = &objs[dat->num_objects++];
                    memcpy(o->type, "BMP ", 4);
                    o->body.bmp = bmp;
                    o->len_uncompressed = o->len_compressed = dat_len_bmp(bmp);
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", basename_portable(argv[i+1]));
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                }

                i++;
                continue;
            }

            /* PAL / ACT */
            if (strcmp(argv[i], "--pal") == 0 && i + 1 < argc) {
                u8 *pal = NULL;
                if (!load_act_to_pal63(argv[i + 1], &pal)) {
                    fprintf(stderr, "ERR: cannot load PAL %s\n", argv[i + 1]);
                    free_allegro_dat(dat);
                    return 2;
                }

                {
                    DatObject *o = &objs[dat->num_objects++];
                    memcpy(o->type, "PAL ", 4);
                    o->body.pal = pal;
                    o->len_uncompressed = o->len_compressed = dat_len_pal();
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", basename_portable(argv[i+1]));
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                }

                i++;
                continue;
            }

            /* RLE (raw Allegro RLE buffer) */
            if (strcmp(argv[i], "--rle") == 0 && i + 1 < argc) {
                FILE *rf = fopen(argv[i + 1], "rb");
                long sz;
                u8 *buf;
                DatRleSprite *r;

                if (!rf) {
                    fprintf(stderr, "ERR: cannot open RLE %s\n", argv[i + 1]);
                    free_allegro_dat(dat);
                    return 2;
                }

                fseek(rf, 0, SEEK_END);
                sz = ftell(rf);
                fseek(rf, 0, SEEK_SET);

                buf = (u8*)malloc(sz);
                if (!buf) {
                    fclose(rf);
                    free_allegro_dat(dat);
                    return 2;
                }

                if (fread(buf, 1, (size_t)sz, rf) != (size_t)sz) {
                    fclose(rf);
                    free(buf);
                    free_allegro_dat(dat);
                    return 2;
                }
                fclose(rf);

                r = (DatRleSprite*)calloc(1, sizeof(DatRleSprite));
                r->bits_per_pixel = 8;  /* Allegro RLE usually 8bpp */
                r->width  = 0; /* Unknown if only buffer provided */
                r->height = 0;
                r->len_image = (u32)sz;
                r->image = buf;

                {
                    DatObject *o = &objs[dat->num_objects++];
                    memcpy(o->type, "RLE ", 4);
                    o->body.rle = r;
                    o->len_uncompressed = o->len_compressed = dat_len_rle(r);
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", basename_portable(argv[i+1]));
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                }

                i++;
                continue;
            }

            /* FONT 8×8 */
            if (strcmp(argv[i], "--font8-bmp") == 0 && i + 1 < argc) {
                DatFont *font = NULL;
                if (!build_font8_from_bmp(argv[i+1], 128, &font)) {
                    fprintf(stderr, "ERR: cannot build FONT8 from %s\n", argv[i + 1]);
                    free_allegro_dat(dat);
                    return 2;
                }

                {
                    DatObject *o = &objs[dat->num_objects++];
                    memcpy(o->type, "FONT", 4);
                    o->body.font = font;
                    o->len_uncompressed = o->len_compressed = dat_len_font(font);
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", basename_portable(argv[i+1]));
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                }

                i++;
                continue;
            }

            /* FONT 8×16 */
            if (strcmp(argv[i], "--font16-bmp") == 0 && i + 1 < argc) {
                DatFont *font = NULL;
                if (!build_font16_from_bmp(argv[i+1], 128, &font)) {
                    fprintf(stderr, "ERR: cannot build FONT16 from %s\n", argv[i + 1]);
                    free_allegro_dat(dat);
                    return 2;
                }

                {
                    DatObject *o = &objs[dat->num_objects++];
                    memcpy(o->type, "FONT", 4);
                    o->body.font = font;
                    o->len_uncompressed = o->len_compressed = dat_len_font(font);
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", basename_portable(argv[i+1]));
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                }

                i++;
                continue;
            }

            /* unknown */
            fprintf(stderr, "Unknown or malformed arg: %s\n", argv[i]);
            usage();
            free_allegro_dat(dat);
            return 1;
        } /* end for all args */

        if (dat->num_objects == 0) {
            fprintf(stderr, "No objects to write.\n");
            free_allegro_dat(dat);
            return 1;
        }

        if (!dat_write(out, dat)) {
            fprintf(stderr, "Write failed.\n");
            free_allegro_dat(dat);
            return 2;
        }

        printf("DAT written: %s (%u objects)\n", out, dat->num_objects);

        free_allegro_dat(dat);
        return 0;
    }
}
