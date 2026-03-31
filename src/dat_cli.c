/* src/dat_cli.c  (v4.0 - New CLI Interface) */
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

/* ------------------------------------------------------------------ */
/* Basic utilities                                                     */
/* ------------------------------------------------------------------ */

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static void set_prop(Property *p, const char type4[4], const char *value) {
    memcpy(p->magic, "prop", 4);
    memcpy(p->type, type4, 4);
    p->len_body = (u32)strlen(value);
    p->body = dupstr(value);
}

static const char *basename_portable(const char *path) {
    const char *s  = strrchr(path, '/');
    const char *s2 = strrchr(path, '\\');
    if (!s || (s2 && s2 > s)) s = s2;
    return s ? s + 1 : path;
}

/* Converts "musica.mid" to "MUSICA_MID" so Allegro can find it */
static void sanitize_allegro_name(char *dest, const char *path) {
    const char *b = basename_portable(path);
    int i = 0;
    while (b[i] && i < 31) {
        dest[i] = (b[i] == '.') ? '_' : (char)toupper((unsigned char)b[i]);
        i++;
    }
    dest[i] = '\0';
}

/* Exact date format from small.dat: "m-dd-yyyy, h:mm" */
static void now_datestr(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    if (!tmv) { if (n) buf[0] = '\0'; return; }
    strftime(buf, n, "%m-%d-%Y, %H:%M", tmv);
    /* Remove leading zeros from month and hour */
    if (buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
    {
        char *colon = strchr(buf, ',');
        if (colon && colon[2] == '0')
            memmove(colon + 2, colon + 3, strlen(colon + 3) + 1);
    }
}

/* Portable case-insensitive string comparison (without strcasecmp) */
static int str_iequal(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* ------------------------------------------------------------------ */
/* Auto-detection of type by file extension                            */
/* ------------------------------------------------------------------ */

static const char *detect_type_from_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "DATA";
    dot++; /* points to the text after the dot */

    if (str_iequal(dot, "bmp"))                    return "BMP";
    if (str_iequal(dot, "rle"))                    return "RLE";
    if (str_iequal(dot, "mid") ||
        str_iequal(dot, "midi"))                   return "MIDI";
    if (str_iequal(dot, "wav"))                    return "SAMP";
    if (str_iequal(dot, "fli") ||
        str_iequal(dot, "flc"))                    return "FLIC";
    if (str_iequal(dot, "act") ||
        str_iequal(dot, "pal"))                    return "PAL";

    return "DATA";
}

/* ------------------------------------------------------------------ */
/* Helper: initializes the 3 standard properties of an object         */
/* ------------------------------------------------------------------ */

static void init_object_props(DatObject *o, const char type[4], s32 size,
                              const char *filepath, const char *datebuf) {
    char clean_name[64];
    memcpy(o->type, type, 4);
    o->len_uncompressed = o->len_compressed = size;
    o->num_properties = 3;
    o->properties = (Property *)calloc(3, sizeof(Property));
    set_prop(&o->properties[0], "DATE", datebuf);
    sanitize_allegro_name(clean_name, basename_portable(filepath));
    set_prop(&o->properties[1], "NAME", clean_name);
    set_prop(&o->properties[2], "ORIG", filepath);
}

/* ------------------------------------------------------------------ */
/* find_or_alloc_slot: returns the slot where the object is written.  */
/* If an object with the same sanitized NAME already exists, it frees */
/* it and reuses that slot (overwrite in-place). Otherwise, uses      */
/* num_objects++.                                                      */
/* ------------------------------------------------------------------ */

static DatObject *find_or_alloc_slot(AllegroDat *dat, DatObject *objs,
                                     const char *filepath) {
    char new_name[64];
    u32 i;
    sanitize_allegro_name(new_name, basename_portable(filepath));

    for (i = 0; i < dat->num_objects; i++) {
        DatObject *o = &objs[i];
        /* The NAME property is always at index 1 (DATE=0, NAME=1, ORIG=2) */
        if (o->num_properties >= 2 && o->properties[1].body &&
            strcmp(o->properties[1].body, new_name) == 0) {
            fprintf(stderr, "Info: overwriting existing '%s' in the DAT\n", new_name);
            free_dat_object(o);
            memset(o, 0, sizeof(DatObject));
            return o; /* slot reutilizado, num_objects no cambia */
        }
    }

    /* New object: use next slot */
    return &objs[dat->num_objects++];
}

/* ------------------------------------------------------------------ */
/* add_file: central type dispatcher                                   */
/* ------------------------------------------------------------------ */

static int add_file(AllegroDat *dat, DatObject *objs,
                    const char *filepath, const char *type_str,
                    const char *datebuf) {
    DatObject *o;

    /* --- BMP --- */
    if (str_iequal(type_str, "BMP")) {
        DatBitmap *bmp = NULL;
        if (!load_bmp_to_dat_bitmap(filepath, &bmp)) {
            fprintf(stderr, "Error: could not load BMP '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.bmp = bmp;
        init_object_props(o, "BMP ", (s32)(2+2+2 +
            (bmp->width * bmp->height * ((u32)bmp->bits_per_pixel / 8u))),
            filepath, datebuf);
        return 1;
    }

    /* --- PAL --- */
    if (str_iequal(type_str, "PAL")) {
        u8 *pal = NULL;
        const char *dot = strrchr(filepath, '.');
        int is_bmp = dot && str_iequal(dot + 1, "bmp");
        int ok = is_bmp ? load_bmp_to_pal63(filepath, &pal)
                        : load_act_to_pal63(filepath, &pal);
        if (!ok) {
            fprintf(stderr, "Error: could not load PAL '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.pal = pal;
        init_object_props(o, "PAL ", 256 * 4, filepath, datebuf);
        return 1;
    }

    /* --- RLE --- */
    if (str_iequal(type_str, "RLE")) {
        u8 *buf; u32 sz;
        DatRleSprite *r;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        r = (DatRleSprite *)calloc(1, sizeof(DatRleSprite));
        r->bits_per_pixel = 8; r->len_image = sz; r->image = buf;
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.rle = r;
        init_object_props(o, "RLE ", (s32)(2+2+2+4) + (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- FONT8 / FONT (alias FONT8) --- */
    if (str_iequal(type_str, "FONT8") || str_iequal(type_str, "FONT")) {
        DatFont *font = NULL;
        if (!build_font8_from_bmp(filepath, 128, &font)) {
            fprintf(stderr, "Error: could not build FONT8 from '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.font = font;
        init_object_props(o, "FONT", 2 + 95 * 8, filepath, datebuf);
        return 1;
    }

    /* --- FONT16 --- */
    if (str_iequal(type_str, "FONT16")) {
        DatFont *font = NULL;
        if (!build_font16_from_bmp(filepath, 128, &font)) {
            fprintf(stderr, "Error: could not build FONT16 from '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.font = font;
        init_object_props(o, "FONT", 2 + 95 * 16, filepath, datebuf);
        return 1;
    }

    /* --- MIDI --- */
    if (str_iequal(type_str, "MIDI")) {
        u8 *raw; u32 raw_sz;
        u8 *alg_buf = NULL;
        unsigned int alg_sz = 0;
        if (!load_file_bytes(filepath, &raw, &raw_sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        if (!mid_to_allegro_dat(raw, raw_sz, &alg_buf, &alg_sz)) {
            fprintf(stderr, "Error: could not convert '%s' to Allegro MIDI format\n", filepath);
            free(raw);
            return 0;
        }
        free(raw);
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = alg_buf;
        init_object_props(o, "MIDI", (s32)alg_sz, filepath, datebuf);
        return 1;
    }

    /* --- SAMP / WAV --- */
    if (str_iequal(type_str, "SAMP") || str_iequal(type_str, "WAV")) {
        u8 *raw; u32 raw_sz;
        u8 *alg_buf = NULL;
        unsigned int alg_sz = 0;
        if (!load_file_bytes(filepath, &raw, &raw_sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        if (!wav_to_allegro_samp(raw, raw_sz, &alg_buf, &alg_sz)) {
            fprintf(stderr, "Error: could not convert '%s' to Allegro SAMP format\n", filepath);
            free(raw);
            return 0;
        }
        free(raw);
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = alg_buf;
        init_object_props(o, "SAMP", (s32)alg_sz, filepath, datebuf);
        return 1;
    }

    /* --- FLIC --- */
    if (str_iequal(type_str, "FLIC")) {
        u8 *buf; u32 sz;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        if (sz < 6 || !((buf[4] == 0x11 && buf[5] == 0xAF) ||
                         (buf[4] == 0x12 && buf[5] == 0xAF))) {
            fprintf(stderr, "Error: '%s' is not a valid FLI/FLC file\n", filepath);
            free(buf);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, "FLIC", (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- DATA (generic blob) --- */
    if (str_iequal(type_str, "DATA")) {
        u8 *buf; u32 sz;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, "DATA", (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- CMP --- */
    if (str_iequal(type_str, "CMP")) {
        u8 *buf; u32 sz;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, "CMP ", (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- XCMP --- */
    if (str_iequal(type_str, "XCMP")) {
        u8 *buf; u32 sz;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, "XCMP", (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- PAT --- */
    if (str_iequal(type_str, "PAT")) {
        u8 *buf; u32 sz;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, "PAT ", (s32)sz, filepath, datebuf);
        return 1;
    }

    /* --- Generic type: first 4 chars uppercase, pad with spaces --- */
    {
        u8 *buf; u32 sz;
        char tag[4];
        int k;
        if (!load_file_bytes(filepath, &buf, &sz)) {
            fprintf(stderr, "Error: could not read '%s'\n", filepath);
            return 0;
        }
        for (k = 0; k < 4; k++)
            tag[k] = (type_str[k] != '\0') ? (char)toupper((unsigned char)type_str[k]) : ' ';
        o = find_or_alloc_slot(dat, objs, filepath);
        o->body.any = buf;
        init_object_props(o, tag, (s32)sz, filepath, datebuf);
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/* usage                                                               */
/* ------------------------------------------------------------------ */

static void usage(void) {
    printf("\nAllegro 4 DAT tool (ANSI C)\n\n");
    printf("Usage:\n");
    printf("  dat <archivo.dat> -l\n");
    printf("  dat <archivo.dat> [-t TYPE] -a <file> [[-t TYPE] -a <file> ...] [--h header.h]\n\n");
    printf("Options:\n");
    printf("  -l            List contents of an existing .dat file\n");
    printf("  -a <file>     Add file (type auto-detected from extension)\n");
    printf("  -t TYPE       Force type for the next -a only, then resets\n");
    printf("  --h <file>    Generate C header with #define indices\n\n");
    printf("Auto-detected types by extension:\n");
    printf("  .bmp -> BMP | .rle -> RLE | .mid/.midi -> MIDI\n");
    printf("  .wav -> SAMP | .fli/.flc -> FLIC | .act/.pal -> PAL\n");
    printf("  (other) -> DATA\n\n");
    printf("Explicit -t types: BMP PAL RLE FONT8 FONT16 FONT MIDI SAMP WAV FLIC DATA CMP XCMP PAT\n\n");
}

/* ------------------------------------------------------------------ */
/* dat_list: reads and displays the contents of a .dat file           */
/* ------------------------------------------------------------------ */

static u32 read_be32_buf(const u8 *buf, u32 bufsz, u32 *pos) {
    u32 v;
    if (*pos + 4 > bufsz) return 0;
    v = ((u32)buf[*pos] << 24) | ((u32)buf[*pos+1] << 16)
      | ((u32)buf[*pos+2] <<  8) |  (u32)buf[*pos+3];
    *pos += 4;
    return v;
}

static const char *find_prop(const u8 *buf, u32 obj_start, u32 bufsz,
                              const char want[4]) {
    u32 pos = obj_start;
    while (pos + 12 <= bufsz && memcmp(buf + pos, "prop", 4) == 0) {
        u32 plen;
        pos += 4;
        if (memcmp(buf + pos, want, 4) == 0) {
            pos += 4;
            plen = read_be32_buf(buf, bufsz, &pos);
            return (plen > 0 && pos + plen <= bufsz) ? (const char *)(buf + pos) : NULL;
        }
        pos += 4;
        plen = read_be32_buf(buf, bufsz, &pos);
        pos += plen;
    }
    return NULL;
}

static u32 find_prop_len(const u8 *buf, u32 obj_start, u32 bufsz,
                         const char want[4]) {
    u32 pos = obj_start;
    while (pos + 12 <= bufsz && memcmp(buf + pos, "prop", 4) == 0) {
        u32 plen;
        pos += 4;
        if (memcmp(buf + pos, want, 4) == 0) {
            pos += 4;
            plen = read_be32_buf(buf, bufsz, &pos);
            return plen;
        }
        pos += 4;
        plen = read_be32_buf(buf, bufsz, &pos);
        pos += plen;
    }
    return 0;
}

static const char *type_description(const char tag[4]) {
    if (memcmp(tag, "BMP ", 4) == 0) return "Bitmap";
    if (memcmp(tag, "PAL ", 4) == 0) return "Palette";
    if (memcmp(tag, "RLE ", 4) == 0) return "RLE Sprite";
    if (memcmp(tag, "CMP ", 4) == 0) return "Compiled Sprite";
    if (memcmp(tag, "XCMP", 4) == 0) return "Mode-X Sprite";
    if (memcmp(tag, "FONT", 4) == 0) return "Font";
    if (memcmp(tag, "SAMP", 4) == 0) return "Sample";
    if (memcmp(tag, "MIDI", 4) == 0) return "MIDI";
    if (memcmp(tag, "FLIC", 4) == 0) return "FLI/FLC Anim";
    if (memcmp(tag, "DATA", 4) == 0) return "Data";
    if (memcmp(tag, "FILE", 4) == 0) return "Sub-datafile";
    if (memcmp(tag, "PAT ", 4) == 0) return "Gravis Patch";
    if (memcmp(tag, "info", 4) == 0) return "(grabber info)";
    return "Unknown";
}

static void print_type_detail(const char tag[4], const u8 *body, u32 body_sz) {
    if (body_sz == 0) return;

    if ((memcmp(tag, "BMP ", 4) == 0 ||
         memcmp(tag, "CMP ", 4) == 0 ||
         memcmp(tag, "XCMP", 4) == 0) && body_sz >= 6) {
        s16 bits = (s16)(((u16)body[0] << 8) | body[1]);
        u16 w    = (u16)(((u16)body[2] << 8) | body[3]);
        u16 h    = (u16)(((u16)body[4] << 8) | body[5]);
        printf("  %dx%d, %d bpp", w, h, (int)bits);
        return;
    }
    if (memcmp(tag, "RLE ", 4) == 0 && body_sz >= 8) {
        s16 bits = (s16)(((u16)body[0] << 8) | body[1]);
        u16 w    = (u16)(((u16)body[2] << 8) | body[3]);
        u16 h    = (u16)(((u16)body[4] << 8) | body[5]);
        printf("  %dx%d, %d bpp", w, h, (int)bits);
        return;
    }
    if (memcmp(tag, "SAMP", 4) == 0 && body_sz >= 8) {
        s16 bits = (s16)(((u16)body[0] << 8) | body[1]);
        u16 freq = (u16)(((u16)body[2] << 8) | body[3]);
        s32 len  = (s32)(((u32)body[4] << 24) | ((u32)body[5] << 16)
                       | ((u32)body[6] <<  8) |  (u32)body[7]);
        printf("  %d Hz, %d-bit, %s, %d frames",
               (int)freq, (int)(bits < 0 ? -bits : bits),
               bits < 0 ? "stereo" : "mono", (int)len);
        return;
    }
    if (memcmp(tag, "MIDI", 4) == 0 && body_sz >= 2) {
        s16 div = (s16)(((u16)body[0] << 8) | body[1]);
        int tracks = 0, t;
        u32 mpos = 2;
        for (t = 0; t < 32 && mpos + 4 <= body_sz; t++) {
            s32 tlen = (s32)(((u32)body[mpos] << 24) | ((u32)body[mpos+1] << 16)
                           | ((u32)body[mpos+2] <<  8) |  (u32)body[mpos+3]);
            mpos += 4;
            if (tlen > 0) { tracks++; mpos += (u32)tlen; }
        }
        printf("  %d tracks, %d ticks/beat", tracks, (int)div);
        return;
    }
    if (memcmp(tag, "FONT", 4) == 0 && body_sz >= 2) {
        s16 sz = (s16)(((u16)body[0] << 8) | body[1]);
        if (sz == 8)       printf("  8x8 bitmap");
        else if (sz == 16) printf("  8x16 bitmap");
        else if (sz == 0)  printf("  proportional (3.9+ format)");
        else if (sz == -1) printf("  proportional (legacy)");
        else               printf("  size=%d", (int)sz);
        return;
    }
    if (memcmp(tag, "FLIC", 4) == 0 && body_sz >= 12) {
        u16 magic  = (u16)(body[4] | ((u16)body[5] << 8));
        u16 frames = (u16)(body[6] | ((u16)body[7] << 8));
        u16 w      = (u16)(body[8] | ((u16)body[9] << 8));
        u16 h      = (u16)(body[10]| ((u16)body[11]<< 8));
        printf("  %dx%d, %d frames (%s)",
               (int)w, (int)h, (int)frames,
               magic == 0xAF12 ? "FLC" : "FLI");
        return;
    }
}

/* ------------------------------------------------------------------ */
/* dat_load_existing: loads an existing .dat file into the object array. */
/* Objects are stored as verbatim blobs (body.any = bytes already in     */
/* Allegro format), with their original properties preserved.           */
/* The GrabberInfo object ("info") is discarded; a new one will be added.*/
/* Returns the number of objects loaded, or -1 if it fails.             */
/* ------------------------------------------------------------------ */

static int dat_load_existing(const char *filename, DatObject *objs, u32 max_objs) {
    u8  *buf;
    u32  bufsz, pos, pack_magic, dat_magic, num_objects, idx;
    int  loaded = 0;

    if (!load_file_bytes(filename, &buf, &bufsz)) return 0; /* new file, OK */
    if (bufsz < 12) { free(buf); fprintf(stderr, "Warning: '%s' too small, ignoring\n", filename); return 0; }

    pos = 0;
    pack_magic  = read_be32_buf(buf, bufsz, &pos);
    dat_magic   = read_be32_buf(buf, bufsz, &pos);
    num_objects = read_be32_buf(buf, bufsz, &pos);

    if (!(pack_magic == 0x736C682Eu && dat_magic == 0x414C4C2Eu)) {
        free(buf);
        fprintf(stderr, "Error: '%s' is not a valid Allegro DAT\n", filename);
        return -1;
    }

    for (idx = 0; idx < num_objects && (u32)loaded < max_objs; idx++) {
        u32  obj_start = pos;
        u32  num_props = 0;
        char type_tag[4];
        u32  len_compressed, len_uncompressed;

        /* Contar y avanzar propiedades */
        {
            u32 scan = pos;
            while (scan + 12 <= bufsz && memcmp(buf + scan, "prop", 4) == 0) {
                u32 plen;
                scan += 8;
                plen = read_be32_buf(buf, bufsz, &scan);
                scan += plen;
                num_props++;
            }
            pos = scan;
        }

        if (pos + 12 > bufsz) break;

        memcpy(type_tag, buf + pos, 4);
        pos += 4;
        len_compressed   = read_be32_buf(buf, bufsz, &pos);
        len_uncompressed = read_be32_buf(buf, bufsz, &pos);

        /* Descartar GrabberInfo */
        if (memcmp(type_tag, "info", 4) == 0) {
            pos += len_compressed;
            continue;
        }

        /* Reconstruir propiedades */
        {
            DatObject *o = &objs[loaded];
            u32 p, scan2 = obj_start;
            memcpy(o->type, type_tag, 4);
            o->len_compressed   = (s32)len_compressed;
            o->len_uncompressed = (s32)len_uncompressed;
            o->num_properties   = (int)num_props;
            o->properties = (Property *)calloc(num_props + 1, sizeof(Property));

            for (p = 0; p < num_props && scan2 + 12 <= bufsz; p++) {
                Property *pr = &o->properties[p];
                u32 plen;
                memcpy(pr->magic, buf + scan2, 4); scan2 += 4;
                memcpy(pr->type,  buf + scan2, 4); scan2 += 4;
                plen = read_be32_buf(buf, bufsz, &scan2);
                pr->len_body = plen;
                if (plen > 0 && scan2 + plen <= bufsz) {
                    pr->body = (char *)malloc(plen + 1);
                    memcpy(pr->body, buf + scan2, plen);
                    pr->body[plen] = '\0';
                } else {
                    pr->body = dupstr("");
                }
                scan2 += plen;
            }

            /* Cuerpo: reconstruir struct o copiar verbatim segun tipo.
               dat_write() despacha por type[4], por lo que BMP/PAL/RLE/FONT
               deben tener sus structs propias; el resto va como verbatim. */
            if (len_compressed > 0 && pos + len_compressed <= bufsz) {
                const u8 *bd = buf + pos;
                u32 bdsz = len_compressed;

                if (memcmp(type_tag, "BMP ", 4) == 0 && bdsz >= 6) {
                    /* Formato en disco: s16 bpp, u16 w, u16 h, pixels[] (big-endian) */
                    DatBitmap *bmp = (DatBitmap *)calloc(1, sizeof(DatBitmap));
                    bmp->bits_per_pixel = (s16)(((u16)bd[0] << 8) | bd[1]);
                    bmp->width          = (u16)(((u16)bd[2] << 8) | bd[3]);
                    bmp->height         = (u16)(((u16)bd[4] << 8) | bd[5]);
                    {
                        u32 px_sz = (u32)bmp->width * bmp->height * ((u32)(bmp->bits_per_pixel < 0 ? -bmp->bits_per_pixel : bmp->bits_per_pixel) / 8u);
                        bmp->image = (u8 *)malloc(px_sz + 1);
                        if (bdsz >= 6 + px_sz)
                            memcpy(bmp->image, bd + 6, px_sz);
                    }
                    o->body.bmp = bmp;

                } else if (memcmp(type_tag, "PAL ", 4) == 0 && bdsz >= 3) {
                    /* Formato en disco: 256 x { R, G, B, pad }. Guardamos solo RGB (3 bytes x 256) */
                    u8 *pal = (u8 *)malloc(256 * 3);
                    u32 pi;
                    for (pi = 0; pi < 256 && (pi * 4 + 3) < bdsz; pi++)
                        memcpy(pal + pi * 3, bd + pi * 4, 3);
                    o->body.pal = pal;

                } else if (memcmp(type_tag, "RLE ", 4) == 0 && bdsz >= 10) {
                    /* Formato: s16 bpp, u16 w, u16 h, u32 len_image, image[] */
                    DatRleSprite *r = (DatRleSprite *)calloc(1, sizeof(DatRleSprite));
                    r->bits_per_pixel = (s16)(((u16)bd[0] << 8) | bd[1]);
                    r->width          = (u16)(((u16)bd[2] << 8) | bd[3]);
                    r->height         = (u16)(((u16)bd[4] << 8) | bd[5]);
                    r->len_image      = ((u32)bd[6] << 24) | ((u32)bd[7] << 16) | ((u32)bd[8] << 8) | bd[9];
                    r->image = (u8 *)malloc(r->len_image + 1);
                    if (bdsz >= 10 + r->len_image)
                        memcpy(r->image, bd + 10, r->len_image);
                    o->body.rle = r;

                } else if (memcmp(type_tag, "FONT", 4) == 0 && bdsz >= 2) {
                    /* Formato: s16 font_size, chars[] */
                    DatFont *font = (DatFont *)calloc(1, sizeof(DatFont));
                    font->font_size = (s16)(((u16)bd[0] << 8) | bd[1]);
                    if (font->font_size == 8 && bdsz >= 2 + 95 * 8) {
                        DatFont8 *f8 = (DatFont8 *)calloc(1, sizeof(DatFont8));
                        memcpy(f8->chars, bd + 2, 95 * 8);
                        font->u.f8 = f8;
                    } else if (font->font_size == 16 && bdsz >= 2 + 95 * 16) {
                        DatFont16 *f16 = (DatFont16 *)calloc(1, sizeof(DatFont16));
                        memcpy(f16->chars, bd + 2, 95 * 16);
                        font->u.f16 = f16;
                    }
                    o->body.font = font;

                } else {
                    /* MIDI, SAMP, FLIC, DATA, CMP, XCMP, PAT y cualquier otro: verbatim */
                    u8 *body_copy = (u8 *)malloc(bdsz);
                    memcpy(body_copy, bd, bdsz);
                    o->body.any = body_copy;
                }
            } else {
                o->body.any = NULL;
            }

            loaded++;
        }

        pos += len_compressed;
    }

    free(buf);
    return loaded;
}

static int dat_list(const char *filename) {
    u8  *buf;
    u32  bufsz, pos, pack_magic, dat_magic, num_objects, idx;
    int  name_w = 32;

    if (!load_file_bytes(filename, &buf, &bufsz)) {
        fprintf(stderr, "Error: no se puede abrir '%s'\n", filename);
        return 1;
    }
    if (bufsz < 12) { free(buf); fprintf(stderr, "Error: file too small\n"); return 1; }

    pos = 0;
    pack_magic  = read_be32_buf(buf, bufsz, &pos);
    dat_magic   = read_be32_buf(buf, bufsz, &pos);
    num_objects = read_be32_buf(buf, bufsz, &pos);

    if (pack_magic == 0x736C682Eu && dat_magic == 0x414C4C2Eu) {
        /* F_NOPACK_MAGIC + DAT_MAGIC — no compression, OK */
    } else if (pack_magic == 0x736C6821u) {
        fprintf(stderr, "Error: '%s' is compressed with LZSS (not supported)\n", filename);
        free(buf); return 1;
    } else {
        fprintf(stderr, "Error: '%s' is not a valid Allegro DAT file\n", filename);
        free(buf); return 1;
    }

    printf("File: %s\n", filename);
    printf("Objects: %u\n\n", num_objects);
    printf("%-4s  %-*s  %-14s  %10s  %s\n",
           "#", name_w, "Name", "Type", "Size", "Details");
    printf("%-4s  %-*s  %-14s  %10s  %s\n",
           "----", name_w, "--------------------------------",
           "--------------", "----------", "-------");

    for (idx = 0; idx < num_objects; idx++) {
        u32         obj_start = pos;
        const char *name_ptr, *date_ptr;
        u32         name_len, date_len;
        char        name_buf[64], date_buf[32], type_tag[5];
        u32         len_compressed, len_uncompressed;
        const u8   *body;

        /* Saltar propiedades para llegar al type tag */
        {
            u32 scan = pos;
            while (scan + 12 <= bufsz && memcmp(buf + scan, "prop", 4) == 0) {
                u32 plen;
                scan += 8;
                plen = read_be32_buf(buf, bufsz, &scan);
                scan += plen;
            }
            pos = scan;
        }

        if (pos + 12 > bufsz) break;

        memcpy(type_tag, buf + pos, 4); type_tag[4] = '\0';
        pos += 4;
        len_compressed   = read_be32_buf(buf, bufsz, &pos);
        len_uncompressed = read_be32_buf(buf, bufsz, &pos);
        body = buf + pos;

        name_ptr = find_prop(buf, obj_start, bufsz, "NAME");
        name_len = find_prop_len(buf, obj_start, bufsz, "NAME");
        if (name_ptr && name_len > 0) {
            u32 copy = name_len < 63 ? name_len : 63;
            memcpy(name_buf, name_ptr, copy); name_buf[copy] = '\0';
        } else {
            strcpy(name_buf, "(sin nombre)");
        }

        date_ptr = find_prop(buf, obj_start, bufsz, "DATE");
        date_len = find_prop_len(buf, obj_start, bufsz, "DATE");
        if (date_ptr && date_len > 0) {
            u32 copy = date_len < 31 ? date_len : 31;
            memcpy(date_buf, date_ptr, copy); date_buf[copy] = '\0';
        } else {
            date_buf[0] = '\0';
        }

        printf("%-4u  %-*s  %-14s  %10u",
               idx + 1, name_w, name_buf,
               type_description(type_tag), len_uncompressed);

        {
            u32 safe_body_sz = len_uncompressed;
            if (pos + safe_body_sz > bufsz)
                safe_body_sz = bufsz - pos;
            print_type_detail(type_tag, body, safe_body_sz);
        }

        if (date_buf[0]) printf("  [%s]", date_buf);
        printf("\n");

        pos += len_compressed;
        (void)len_uncompressed;
    }

    printf("\n");
    free(buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    int i;
    const char *dat_file;
    const char *header_file  = NULL;
    const char *forced_type  = NULL;
    int         added_any    = 0;
    char        datebuf[64];
    AllegroDat *dat;
    DatObject  *objs;

    /* 1. Validacion minima */
    if (argc < 3) { usage(); return 1; }

    dat_file = argv[1];
    now_datestr(datebuf, sizeof(datebuf));

    /* 2. Preparar estructura DAT y cargar contenido existente (append) */
    dat = (AllegroDat *)calloc(1, sizeof(AllegroDat));
    dat->pack_magic = 0x736C682Eu; /* F_NOPACK_MAGIC */
    dat->dat_magic  = 0x414C4C2Eu; /* DAT_MAGIC      */
    objs = (DatObject *)calloc(1024, sizeof(DatObject));
    dat->objects = objs;

    {
        int preloaded = dat_load_existing(dat_file, objs, 1022);
        if (preloaded < 0) { free(objs); free(dat); return 1; }
        dat->num_objects = (u32)preloaded;
    }

    /* 3. Iterar argumentos desde argv[2] */
    for (i = 2; i < argc; i++) {

        /* -l : listar */
        if (strcmp(argv[i], "-l") == 0) {
            free(objs);
            free(dat);
            return dat_list(dat_file);
        }

        /* -t TYPE : forzar tipo para el proximo -a */
        if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -t requires a TYPE argument\n");
                free(objs); free(dat); return 1;
            }
            forced_type = argv[++i];
            continue;
        }

        /* -a FILE : add file */
        if (strcmp(argv[i], "-a") == 0) {
            const char *filepath;
            const char *type_str;
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -a requires a file\n");
                free(objs); free(dat); return 1;
            }
            filepath = argv[++i];
            type_str = forced_type ? forced_type
                                   : detect_type_from_extension(filepath);
            add_file(dat, objs, filepath, type_str, datebuf);
            forced_type = NULL; /* reset: only applies to one -a */
            added_any = 1;
            continue;
        }

        /* --h FILE : C header */
        if (strcmp(argv[i], "--h") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --h requires an output file\n");
                free(objs); free(dat); return 1;
            }
            header_file = argv[++i];
            continue;
        }

        fprintf(stderr, "Warning: unknown option '%s'\n", argv[i]);
    }

    /* 4. If no -a was provided, show usage */
    if (!added_any) { usage(); free(objs); free(dat); return 1; }

    /* 5. Final object GrabberInfo */
    {
        DatObject *o = &objs[dat->num_objects++];
        memcpy(o->type, "info", 4);
        o->body.any = dupstr("For internal use by the grabber");
        o->len_uncompressed = o->len_compressed = (s32)strlen((char *)o->body.any);
        o->num_properties = 1;
        o->properties = (Property *)calloc(1, sizeof(Property));
        set_prop(&o->properties[0], "NAME", "GrabberInfo");
    }

    /* 6. Generate header if requested */
    if (header_file) {
        FILE *hf = fopen(header_file, "w");
        if (!hf) {
            fprintf(stderr, "Error: could not create '%s'\n", header_file);
        } else {
            int j;
            for (j = 0; j < (int)dat->num_objects - 1; j++) {
                fprintf(hf, "#define %-30s\t%-4d\t//%.4s\n",
                        dat->objects[j].properties[1].body, j,
                        dat->objects[j].type);
            }
            fclose(hf);
        }
    }

    /* 7. Escribir DAT */
    if (dat_write(dat_file, dat))
        printf("DAT written: %s (%u objects total)\n", dat_file, dat->num_objects);

    free_allegro_dat(dat);
    return 0;
}
