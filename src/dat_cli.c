/* src/dat_cli.c  (v3.3 - Full Compatibility) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "allegro_dat_structs.h"
#include "dat_loader_bmp.h"
#include "dat_loader_data.h"
#include "dat_loader_font.h"
#include "dat_loader_pal.h"
#include "dat_writer.h"
#include "midi_to_allegro.h"
#include "wav_to_allegro.h"

static char* dupstr(const char* s) {
    size_t n = strlen(s);
    char* d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static void set_prop(Property* p, const char type4[4], const char* value) {
    memcpy(p->magic, "prop", 4);
    memcpy(p->type, type4, 4);
    p->len_body = (u32)strlen(value);
    p->body = dupstr(value);
}

static const char* basename_portable(const char* path) {
    const char* s = strrchr(path, '/');
    const char* s2 = strrchr(path, '\\');
    if (!s || (s2 && s2 > s)) s = s2;
    return s ? s + 1 : path;
}

/* Convierte "musica.mid" en "MUSICA_MID" para que Allegro lo encuentre */
static void sanitize_allegro_name(char* dest, const char* path) {
    const char* b = basename_portable(path);
    int i = 0;
    while (b[i] && i < 31) {
        if (b[i] == '.') dest[i] = '_';
        else dest[i] = (char)toupper((unsigned char)b[i]);
        i++;
    }
    dest[i] = '\0';
}

/* Formato de fecha exacto de small.dat: "d-mm-yyyy, H:MM" */
static void now_datestr(char* buf, size_t n) {
    time_t t = time(NULL);
    struct tm* tmv = localtime(&t);
    if (!tmv) {
        if (n)
            buf[0] = '\0';
        return;
    }
    /* Spec format: "m-dd-yyyy, h:mm" - month and hour without leading zero */
    strftime(buf, n, "%m-%d-%Y, %H:%M", tmv);
    /* Remove leading zeros from month and hour as per spec ("3-03-2026, 9:26") */
    if (buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
    {
        char *colon = strchr(buf, ',');
        if (colon && colon[2] == '0') memmove(colon + 2, colon + 3, strlen(colon + 3) + 1);
    }
}

static void usage(void) {
    printf("\nAllegro 4 DAT creator (ANSI C) - Full Support\n\n");
    printf("Usage:\n  dat create out.dat\n");
    printf("      [--bmp file.bmp]* [--pal file.act]*\n");
    printf("      [--rle file.rle]* [--midi file.mid]*\n");
    printf("      [--font8-bmp f.bmp]* [--font16-bmp f.bmp]*\n");
    printf("      [--data file.bin]* [--wav file.wav]*\n\n");
}

int main(int argc, char** argv) {
    int i;
    const char* out;
    char datebuf[64];
    AllegroDat* dat;
    DatObject* objs;

    if (argc < 3 || strcmp(argv[1], "create") != 0) { usage(); return 1; }
    out = argv[2];
    now_datestr(datebuf, sizeof(datebuf));

    dat = (AllegroDat*)calloc(1, sizeof(AllegroDat));
    dat->pack_magic = 0x736C682Eu; /* 'slh.' */
    dat->dat_magic = 0x414C4C2Eu;  /* 'ALL.' */
    objs = (DatObject*)calloc(1024, sizeof(DatObject));
    dat->objects = objs;

    for (i = 3; i < argc; i++) {
        char clean_name[64];
        
        /* BMP */
        if (strcmp(argv[i], "--bmp") == 0 && i + 1 < argc) {
            DatBitmap* bmp = NULL;
            if (load_bmp_to_dat_bitmap(argv[i+1], &bmp)) {
                DatObject* o = &objs[dat->num_objects++];
                memcpy(o->type, "BMP ", 4); o->body.bmp = bmp;
                o->len_uncompressed = o->len_compressed = (s32)(2+2+2 + (bmp->width * bmp->height * ((u32)bmp->bits_per_pixel / 8u)));
                o->num_properties = 3; o->properties = (Property*)calloc(3, sizeof(Property));
                set_prop(&o->properties[0], "DATE", datebuf);
                sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                set_prop(&o->properties[1], "NAME", clean_name);
                set_prop(&o->properties[2], "ORIG", argv[i+1]);
            }
            i++; continue;
        }

        /* PAL */
        if (strcmp(argv[i], "--pal") == 0 && i + 1 < argc) {
            u8* pal = NULL;
            if (load_act_to_pal63(argv[i+1], &pal)) {
                DatObject* o = &objs[dat->num_objects++];
                memcpy(o->type, "PAL ", 4); o->body.pal = pal;
                o->len_uncompressed = o->len_compressed = 256 * 4; /* Spec: 256 x {R,G,B,pad} */
                o->num_properties = 3; o->properties = (Property*)calloc(3, sizeof(Property));
                set_prop(&o->properties[0], "DATE", datebuf);
                sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                set_prop(&o->properties[1], "NAME", clean_name);
                set_prop(&o->properties[2], "ORIG", argv[i+1]);
            }
            i++; continue;
        }

        /* RLE */
        if (strcmp(argv[i], "--rle") == 0 && i + 1 < argc) {
            u8* buf; u32 sz;
            if (load_file_bytes(argv[i+1], &buf, &sz)) {
                DatRleSprite* r = (DatRleSprite*)calloc(1, sizeof(DatRleSprite));
                r->bits_per_pixel = 8; r->len_image = sz; r->image = buf;
                DatObject* o = &objs[dat->num_objects++];
                memcpy(o->type, "RLE ", 4); o->body.rle = r;
                o->len_uncompressed = o->len_compressed = (s32)(2+2+2+4) + (s32)sz;
                o->num_properties = 3; o->properties = (Property*)calloc(3, sizeof(Property));
                set_prop(&o->properties[0], "DATE", datebuf);
                sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                set_prop(&o->properties[1], "NAME", clean_name);
                set_prop(&o->properties[2], "ORIG", argv[i+1]);
            }
            i++; continue;
        }

        /* FONT 8x8 y 8x16 */
        if ((strcmp(argv[i], "--font8-bmp") == 0 || strcmp(argv[i], "--font16-bmp") == 0) && i + 1 < argc) {
            DatFont* font = NULL;
            int is16 = (argv[i][6] == '1');
            int ok = is16 ? build_font16_from_bmp(argv[i+1], 128, &font) : build_font8_from_bmp(argv[i+1], 128, &font);
            if (ok) {
                DatObject* o = &objs[dat->num_objects++];
                memcpy(o->type, "FONT", 4); o->body.font = font;
                o->len_uncompressed = o->len_compressed = (2 + 95 * (is16 ? 16 : 8));
                o->num_properties = 3; o->properties = (Property*)calloc(3, sizeof(Property));
                set_prop(&o->properties[0], "DATE", datebuf);
                sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                set_prop(&o->properties[1], "NAME", clean_name);
                set_prop(&o->properties[2], "ORIG", argv[i+1]);
            }
            i++; continue;
        }

        /* MIDI: convierte SMF (.mid) al formato interno de Allegro 4 */
        if (strcmp(argv[i], "--midi") == 0 && i + 1 < argc) {
            u8* raw; u32 raw_sz;
            if (load_file_bytes(argv[i+1], &raw, &raw_sz)) {
                u8* alg_buf = NULL;
                unsigned int alg_sz = 0;
                if (mid_to_allegro_dat(raw, raw_sz, &alg_buf, &alg_sz)) {
                    sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                    DatObject* o = &objs[dat->num_objects++];
                    memcpy(o->type, "MIDI", 4);
                    o->body.any = alg_buf;
                    o->len_uncompressed = o->len_compressed = (s32)alg_sz;
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", clean_name);
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                } else {
                    fprintf(stderr, "Error: no se pudo convertir '%s' a formato MIDI de Allegro\n", argv[i+1]);
                }
                free(raw);
            }
            i++; continue;
        }

        /* WAV: convierte RIFF/PCM al formato interno SAMP de Allegro 4 */
        if (strcmp(argv[i], "--wav") == 0 && i + 1 < argc) {
            u8* raw; u32 raw_sz;
            if (load_file_bytes(argv[i+1], &raw, &raw_sz)) {
                u8* alg_buf = NULL;
                unsigned int alg_sz = 0;
                if (wav_to_allegro_samp(raw, raw_sz, &alg_buf, &alg_sz)) {
                    DatObject* o = &objs[dat->num_objects++];
                    memcpy(o->type, "SAMP", 4);
                    o->body.any = alg_buf;
                    o->len_uncompressed = o->len_compressed = (s32)alg_sz;
                    o->num_properties = 3;
                    o->properties = (Property*)calloc(3, sizeof(Property));
                    sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                    set_prop(&o->properties[0], "DATE", datebuf);
                    set_prop(&o->properties[1], "NAME", clean_name);
                    set_prop(&o->properties[2], "ORIG", argv[i+1]);
                } else {
                    fprintf(stderr, "Error: no se pudo convertir '%s' a formato SAMP de Allegro\n", argv[i+1]);
                }
                free(raw);
            }
            i++; continue;
        }

        /* DATA: blob generico */
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            u8* buf; u32 sz;
            if (load_file_bytes(argv[i+1], &buf, &sz)) {
                DatObject* o = &objs[dat->num_objects++];
                memcpy(o->type, "DATA", 4); o->body.any = buf;
                o->len_uncompressed = o->len_compressed = (s32)sz;
                o->num_properties = 3; o->properties = (Property*)calloc(3, sizeof(Property));
                sanitize_allegro_name(clean_name, basename_portable(argv[i+1]));
                set_prop(&o->properties[0], "DATE", datebuf);
                set_prop(&o->properties[1], "NAME", clean_name);
                set_prop(&o->properties[2], "ORIG", argv[i+1]);
            }
            i++; continue;
        }
    }

    /* Objeto final GrabberInfo */
    {
        DatObject* o = &objs[dat->num_objects++];
        memcpy(o->type, "info", 4);
        o->body.any = dupstr("For internal use by the grabber");
        o->len_uncompressed = o->len_compressed = (s32)strlen((char*)o->body.any);
        o->num_properties = 1; o->properties = (Property*)calloc(1, sizeof(Property));
        set_prop(&o->properties[0], "NAME", "GrabberInfo");
    }

    if (dat_write(out, dat)) printf("DAT created: %s (%u objects)\n", out, dat->num_objects);
    free_allegro_dat(dat);
    return 0;
}
