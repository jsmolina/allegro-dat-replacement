// src/allegro_dat_structs.h
// Allegro 4 DAT structures & basic types (writer-side)
#ifndef ALLEGRO_DAT_STRUCTS_H
#define ALLEGRO_DAT_STRUCTS_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8; 
typedef uint16_t u16; 
typedef uint32_t u32; 
typedef int16_t  s16; 
typedef int32_t  s32; 

static inline u16 to_be16(u16 x){ return (u16)((x>>8) | (x<<8)); }
static inline u32 to_be32(u32 x){ return ((x>>24)&0xFF) | ((x>>8)&0xFF00) | ((x<<8)&0xFF0000) | ((x<<24)&0xFF000000); }

// Property ("prop")
typedef struct {
    char magic[4];   // always "prop"
    char type[4];    // e.g. "NAME", "DATE", "ORIG"
    u32  len_body;   // big-endian on disk, host-endian here
    char *body;      // not zero-terminated necessarily
} Property;

// Allegro DAT BMP body
typedef struct {
    s16 bits_per_pixel; // host endian
    u16 width;          // host endian
    u16 height;         // host endian (absolute)
    u8 *image;          // width*height*(bpp/8) bytes, top-down, tightly packed
} DatBitmap;

// Allegro DAT RLE body
typedef struct {
    s16 bits_per_pixel; // host endian
    u16 width;          // host endian
    u16 height;         // host endian
    u32 len_image;      // host endian
    u8 *image;          // Allegro RLE buffer (already encoded)
} DatRleSprite;

// Allegro DAT FONT (only 8 and 16 supported here)
typedef struct { u8 chars[95][8]; }  DatFont8;   // 8x8, 1bpp packed into bytes (MSB=left)
typedef struct { u8 chars[95][16]; } DatFont16;  // 8x16

typedef struct {
    s16 font_size;              // 8 or 16 (or 0 for 3.9, not implemented here)
    union { DatFont8 *f8; DatFont16 *f16; void *raw; } u;
} DatFont;

// DAT Object wrapper
typedef struct {
    Property *properties; // array
    int       num_properties;
    s32       len_compressed;
    s32       len_uncompressed;
    char      type[4]; // "BMP ", "PAL ", "RLE ", "FONT"
    union { DatBitmap *bmp; DatRleSprite *rle; DatFont *font; u8 *pal; void *any; } body;
} DatObject;

// DAT file root
typedef struct {
    u32 pack_magic;   // 0x736C682E -> "slh." (unpacked), big-endian on disk
    u32 dat_magic;    // 'ALL.'
    u32 num_objects;  // number of objects
    DatObject *objects; // array length num_objects
} AllegroDat;

// API
#ifdef __cplusplus
extern "C" {
#endif

void free_property(Property *p);
void free_dat_bitmap(DatBitmap *b);
void free_dat_rle(DatRleSprite *r);
void free_dat_font(DatFont *f);
void free_dat_object(DatObject *o);
void free_allegro_dat(AllegroDat *d);

#ifdef __cplusplus
}
#endif

#endif // ALLEGRO_DAT_STRUCTS_H
