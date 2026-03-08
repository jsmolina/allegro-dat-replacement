/* src/dat_writer.c (v3.3) */
#include "dat_writer.h"
#include <string.h>

static void write_bmp(FILE* f, const DatBitmap* b) {
    s16 bebpp = to_be16((u16)b->bits_per_pixel);
    u16 bew = to_be16(b->width), beh = to_be16(b->height);
    fwrite(&bebpp, 2, 1, f);
    fwrite(&bew, 2, 1, f);
    fwrite(&beh, 2, 1, f);
    fwrite(b->image, 1, (size_t)b->width * b->height, f);
}

static void write_pal(FILE* f, const u8* pal) {
    fwrite(pal, 1, 256 * 3, f);
}

static void write_rle(FILE* f, const DatRleSprite* r) {
    s16 bebpp = to_be16((u16)r->bits_per_pixel);
    u16 bew = to_be16(r->width), beh = to_be16(r->height);
    u32 belen = to_be32(r->len_image);
    fwrite(&bebpp, 2, 1, f);
    fwrite(&bew, 2, 1, f);
    fwrite(&beh, 2, 1, f);
    fwrite(&belen, 4, 1, f);
    fwrite(r->image, 1, r->len_image, f);
}

static void write_font(FILE* f, const DatFont* font) {
    s16 fs = to_be16((u16)font->font_size);
    fwrite(&fs, 2, 1, f);
    if (font->font_size == 8) {
        for (int i = 0; i < 95; i++) fwrite(font->u.f8->chars[i], 1, 8, f);
    } else if (font->font_size == 16) {
        for (int i = 0; i < 95; i++) fwrite(font->u.f16->chars[i], 1, 16, f);
    }
}

int dat_write(const char* filename, AllegroDat* dat) {
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;

    u32 pm = to_be32(dat->pack_magic);
    u32 dm = to_be32(dat->dat_magic);
    u32 no = to_be32(dat->num_objects);
    fwrite(&pm, 4, 1, f); fwrite(&dm, 4, 1, f); fwrite(&no, 4, 1, f);

    for (u32 i = 0; i < dat->num_objects; i++) {
        DatObject* o = &dat->objects[i];
        
        /* Escribir propiedades */
        for (int p = 0; p < o->num_properties; p++) {
            Property* pr = &o->properties[p];
            u32 bl = to_be32(pr->len_body);
            fwrite(pr->magic, 1, 4, f);
            fwrite(pr->type, 1, 4, f);
            fwrite(&bl, 4, 1, f);
            if (pr->len_body) fwrite(pr->body, 1, pr->len_body, f);
        }

        /* Encabezado del objeto */
        fwrite(o->type, 1, 4, f);
        u32 lc = to_be32((u32)o->len_compressed);
        u32 lu = to_be32((u32)o->len_uncompressed);
        fwrite(&lc, 4, 1, f); fwrite(&lu, 4, 1, f);

        /* Cuerpo del objeto según tipo */
        if (!memcmp(o->type, "BMP ", 4)) write_bmp(f, o->body.bmp);
        else if (!memcmp(o->type, "PAL ", 4)) write_pal(f, o->body.pal);
        else if (!memcmp(o->type, "RLE ", 4)) write_rle(f, o->body.rle);
        else if (!memcmp(o->type, "FONT", 4)) write_font(f, o->body.font);
        else if (o->body.any && o->len_uncompressed > 0) {
            /* MIDI, DATA, info */
            fwrite(o->body.any, 1, (size_t)o->len_uncompressed, f);
        }
    }

    fclose(f);
    return 1;
}
