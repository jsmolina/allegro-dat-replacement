#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)

// Structs EXACTAMENTE como en el diagrama
typedef struct {
  char magic[4]; // "prop"
  char type[4];
  uint32_t len_body; // big-endian
  char *body;        // sólo si is_valid
} Property;

typedef struct {
  uint16_t width;  // big-endian
  uint16_t height; // big-endian
  uint8_t *body;   // width * height bytes
} FontChar;

typedef struct {
  uint8_t mono;
  uint32_t start_char; // big-endian
  uint32_t end_char;   // big-endian
  FontChar *chars;     // (end_char - start_char) + 1 elementos
} Range;

typedef struct {
  int16_t num_ranges; // big-endian
  Range *ranges;      // num_ranges elementos
} DatFont39;

typedef struct {
  uint8_t chars[95][16]; // 95 caracteres de 16 bytes
} DatFont16;

typedef struct {
  uint8_t chars[95][8]; // 95 caracteres de 8 bytes
} DatFont8;

typedef struct {
  int16_t font_size; // big-endian
  union {
    DatFont39 *font39; // case 0
    DatFont16 *font16; // case 16
    DatFont8 *font8;   // case 8
  } body;
} DatFont;

typedef struct {
  int16_t bits_per_pixel; // big-endian
  uint16_t width;         // big-endian
  uint16_t height;        // big-endian
  uint8_t *image;         // datos de imagen
} DatBitmap;

typedef struct {
  int16_t bits_per_pixel; // big-endian
  uint16_t width;         // big-endian
  uint16_t height;        // big-endian
  uint32_t len_image;     // big-endian
  uint8_t *image;         // datos RLE
} DatRleSprite;

typedef struct {
  Property *properties;     // repeat until !(_.is_valid)
  int32_t len_compressed;   // big-endian
  int32_t len_uncompressed; // big-endian
  union {
    DatBitmap *bitmap;        // case "BMP "
    DatFont *font;            // case "FONT"
    DatRleSprite *rle_sprite; // case "RLE "
  } body;
  char type[4]; // inferido de properties.last.magic
} DatObject;

typedef struct {
  uint32_t pack_magic;  // 0x414C4C2E "ALL." big-endian
  uint32_t dat_magic;   // big-endian
  uint32_t num_objects; // big-endian
  DatObject *objects;   // num_objects elementos
} AllegroDat;

// BMP

typedef struct {
  uint16_t bfType;      // Debe ser "BM"
  uint32_t bfSize;      // Tamaño total del archivo
  uint16_t bfReserved1; // Reservado, debe ser 0
  uint16_t bfReserved2; // Reservado, debe ser 0
  uint32_t bfOffBits;   // Offset donde empiezan los datos de la imagen
} BITMAPFILEHEADER;

typedef struct {
  uint32_t biSize;         // Tamaño de esta estructura
  int32_t biWidth;         // Ancho en píxeles
  int32_t biHeight;        // Alto en píxeles
  uint16_t biPlanes;       // Siempre 1
  uint16_t biBitCount;     // Bits por píxel (24 para RGB)
  uint32_t biCompression;  // 0 = sin compresión
  uint32_t biSizeImage;    // Tamaño de la imagen (puede ser 0 sin compresión)
  int32_t biXPelsPerMeter; // Resolución horizontal
  int32_t biYPelsPerMeter; // Resolución vertical
  uint32_t biClrUsed;      // Número de colores usados
  uint32_t biClrImportant; // Colores importantes
} BITMAPINFOHEADER;

#pragma pack(pop)

// ==================== FUNCIONES DE LECTURA ====================

uint8_t *read_bytes(uint8_t *buffer, void *dest, size_t count) {
  memcpy(dest, buffer, count);
  return buffer + count;
}

uint8_t *read_u32be(uint8_t *buffer, uint32_t *value) {
  *value = ntohl(*(uint32_t *)buffer);
  return buffer + 4;
}

uint8_t *read_s32be(uint8_t *buffer, int32_t *value) {
  *value = (int32_t)ntohl(*(uint32_t *)buffer);
  return buffer + 4;
}

uint8_t *read_u16be(uint8_t *buffer, uint16_t *value) {
  *value = ntohs(*(uint16_t *)buffer);
  return buffer + 2;
}

uint8_t *read_s16be(uint8_t *buffer, int16_t *value) {
  *value = (int16_t)ntohs(*(uint16_t *)buffer);
  return buffer + 2;
}

uint8_t *read_u8(uint8_t *buffer, uint8_t *value) {
  *value = *buffer;
  return buffer + 1;
}

// Función auxiliar para verificar property válida (como en el diagrama)
int property_is_valid(const Property *prop) {
  return (prop->magic[0] == 'p' && prop->magic[1] == 'r' &&
          prop->magic[2] == 'o' && prop->magic[3] == 'p');
}

// Deserializar Property según diagrama
uint8_t *deserialize_property(uint8_t *buffer, Property *prop) {
  buffer = read_bytes(buffer, prop->magic, 4);

  // Si no es "prop", no es válida - PARAMOS AQUÍ según diagrama
  if (!property_is_valid(prop)) {
    return buffer;
  }

  buffer = read_bytes(buffer, prop->type, 4);
  buffer = read_u32be(buffer, &prop->len_body);

  // Leer body sólo si es válida
  if (prop->len_body > 0) {
    prop->body = (char *)malloc(prop->len_body + 1);
    buffer = read_bytes(buffer, prop->body, prop->len_body);
    prop->body[prop->len_body] = '\0';
  } else {
    prop->body = NULL;
  }

  return buffer;
}

// Deserializar FontChar
uint8_t *deserialize_font_char(uint8_t *buffer, FontChar *font_char) {
  buffer = read_u16be(buffer, &font_char->width);
  buffer = read_u16be(buffer, &font_char->height);

  size_t data_size = font_char->width * font_char->height;
  if (data_size > 0) {
    font_char->body = (uint8_t *)malloc(data_size);
    buffer = read_bytes(buffer, font_char->body, data_size);
  } else {
    font_char->body = NULL;
  }

  return buffer;
}

// Deserializar Range
uint8_t *deserialize_range(uint8_t *buffer, Range *range) {
  buffer = read_u8(buffer, &range->mono);
  buffer = read_u32be(buffer, &range->start_char);
  buffer = read_u32be(buffer, &range->end_char);

  uint32_t char_count = (range->end_char - range->start_char) + 1;
  range->chars = (FontChar *)malloc(char_count * sizeof(FontChar));

  for (uint32_t i = 0; i < char_count; i++) {
    buffer = deserialize_font_char(buffer, &range->chars[i]);
  }

  return buffer;
}

// Deserializar DatFont39
uint8_t *deserialize_dat_font_39(uint8_t *buffer, DatFont39 *font) {
  buffer = read_s16be(buffer, &font->num_ranges);

  if (font->num_ranges > 0) {
    font->ranges = (Range *)malloc(font->num_ranges * sizeof(Range));
    for (int16_t i = 0; i < font->num_ranges; i++) {
      buffer = deserialize_range(buffer, &font->ranges[i]);
    }
  } else {
    font->ranges = NULL;
  }

  return buffer;
}

// Deserializar DatFont16
uint8_t *deserialize_dat_font_16(uint8_t *buffer, DatFont16 *font) {
  for (int i = 0; i < 95; i++) {
    buffer = read_bytes(buffer, font->chars[i], 16);
  }
  return buffer;
}

// Deserializar DatFont8
uint8_t *deserialize_dat_font_8(uint8_t *buffer, DatFont8 *font) {
  for (int i = 0; i < 95; i++) {
    buffer = read_bytes(buffer, font->chars[i], 8);
  }
  return buffer;
}

// Deserializar DatFont
uint8_t *deserialize_dat_font(uint8_t *buffer, DatFont *font) {
  buffer = read_s16be(buffer, &font->font_size);

  switch (font->font_size) {
  case 0:
    font->body.font39 = (DatFont39 *)malloc(sizeof(DatFont39));
    buffer = deserialize_dat_font_39(buffer, font->body.font39);
    break;
  case 16:
    font->body.font16 = (DatFont16 *)malloc(sizeof(DatFont16));
    buffer = deserialize_dat_font_16(buffer, font->body.font16);
    break;
  case 8:
    font->body.font8 = (DatFont8 *)malloc(sizeof(DatFont8));
    buffer = deserialize_dat_font_8(buffer, font->body.font8);
    break;
  default:
    // Tipo de fuente no soportado
    font->body.font39 = NULL;
    break;
  }

  return buffer;
}

// Deserializar DatBitmap
uint8_t *deserialize_dat_bitmap(uint8_t *buffer, DatBitmap *bitmap) {
  buffer = read_s16be(buffer, &bitmap->bits_per_pixel);
  buffer = read_u16be(buffer, &bitmap->width);
  buffer = read_u16be(buffer, &bitmap->height);

  // Calcular tamaño de imagen (simplificado)
  size_t image_size =
      bitmap->width * bitmap->height * (bitmap->bits_per_pixel / 8);
  if (image_size > 0) {
    bitmap->image = (uint8_t *)malloc(image_size);
    buffer = read_bytes(buffer, bitmap->image, image_size);
  } else {
    bitmap->image = NULL;
  }

  return buffer;
}

// Deserializar DatRleSprite
uint8_t *deserialize_dat_rle_sprite(uint8_t *buffer, DatRleSprite *sprite) {
  buffer = read_s16be(buffer, &sprite->bits_per_pixel);
  buffer = read_u16be(buffer, &sprite->width);
  buffer = read_u16be(buffer, &sprite->height);
  buffer = read_u32be(buffer, &sprite->len_image);

  if (sprite->len_image > 0) {
    sprite->image = (uint8_t *)malloc(sprite->len_image);
    buffer = read_bytes(buffer, sprite->image, sprite->len_image);
  } else {
    sprite->image = NULL;
  }

  return buffer;
}

// Deserializar DatObject EXACTO al diagrama
// Deserializar DatObject CORREGIDO
uint8_t *deserialize_dat_object(uint8_t *buffer, DatObject *object) {
  // 1. Leer properties hasta encontrar una inválida
  int prop_count = 0;
  int max_props = 100;

  // Contar properties válidas
  uint8_t *temp_buffer = buffer;
  Property temp_prop;

  while (prop_count < max_props) {
    uint8_t *new_buffer = deserialize_property(temp_buffer, &temp_prop);
    if (!property_is_valid(&temp_prop)) {
      break;
    }
    prop_count++;
    temp_buffer = new_buffer;
    if (temp_prop.body)
      free(temp_prop.body);
  }

  printf("DEBUG: Found %d properties\n", prop_count);

  // Leer properties reales
  if (prop_count > 0) {
    object->properties = (Property *)malloc(prop_count * sizeof(Property));
    for (int i = 0; i < prop_count; i++) {
      buffer = deserialize_property(buffer, &object->properties[i]);
    }
  } else {
    object->properties = NULL;
  }

  // 2. Leer el TYPE del objeto (4 bytes) - ESTO ES LO QUE FALTABA
  buffer = read_bytes(buffer, object->type, 4);
  printf("DEBUG: Object type='%.4s'\n", object->type);

  // 3. Leer lengths comprimido/descomprimido
  buffer = read_s32be(buffer, &object->len_compressed);
  buffer = read_s32be(buffer, &object->len_uncompressed);

  printf("DEBUG: compressed=%d, uncompressed=%d\n", object->len_compressed,
         object->len_uncompressed);

  // 4. Leer body según tipo
  if (strncmp(object->type, "BMP ", 4) == 0) {
    object->body.bitmap = (DatBitmap *)malloc(sizeof(DatBitmap));
    buffer = deserialize_dat_bitmap(buffer, object->body.bitmap);
  } else if (strncmp(object->type, "FONT", 4) == 0) {
    object->body.font = (DatFont *)malloc(sizeof(DatFont));
    buffer = deserialize_dat_font(buffer, object->body.font);
  } else if (strncmp(object->type, "RLE ", 4) == 0) {
    object->body.rle_sprite = (DatRleSprite *)malloc(sizeof(DatRleSprite));
    buffer = deserialize_dat_rle_sprite(buffer, object->body.rle_sprite);
  } else {
    printf("WARNING: Unknown object type '%.4s'\n", object->type);
    // Saltar los datos comprimidos según len_compressed
    if (object->len_compressed > 0) {
      buffer += object->len_compressed;
    }
  }

  return buffer;
}

// Deserializar AllegroDat completo
AllegroDat *deserialize_allegro_dat(uint8_t *buffer, size_t buffer_size) {
  AllegroDat *dat = (AllegroDat *)malloc(sizeof(AllegroDat));

  // Leer cabecera
  buffer = read_u32be(buffer, &dat->pack_magic);
  buffer = read_u32be(buffer, &dat->dat_magic);
  buffer = read_u32be(buffer, &dat->num_objects);

  printf("DEBUG: pack_magic=0x%08X, dat_magic=0x%08X, num_objects=%u\n",
         dat->pack_magic, dat->dat_magic, dat->num_objects);

  // Leer objetos
  if (dat->num_objects > 0) {
    dat->objects = (DatObject *)malloc(dat->num_objects * sizeof(DatObject));
    for (uint32_t i = 0; i < dat->num_objects; i++) {
      printf("DEBUG: Reading object %u/%u\n", i + 1, dat->num_objects);
      buffer = deserialize_dat_object(buffer, &dat->objects[i]);
    }
  } else {
    dat->objects = NULL;
  }

  return dat;
}

// ==================== ITERAR PROPERTIES ====================

void iterate_properties(DatObject *object) {
  if (!object->properties) {
    printf("No properties\n");
    return;
  }

  // Buscar property "NAME" para mostrar el nombre del objeto
  char *object_name = "unnamed";
  int count = 0;

  while (count < 1000 && property_is_valid(&object->properties[count])) {
    Property *prop = &object->properties[count];
    if (strncmp(prop->type, "NAME", 4) == 0 && prop->body) {
      object_name = prop->body;
    }
    count++;
  }

  printf("Object '%s' (type='%.4s') - Properties (%d):\n", object_name,
         object->type, count);

  for (int i = 0; i < count; i++) {
    Property *prop = &object->properties[i];
    printf("  [%d] type='%.4s' len=%u", i, prop->type, prop->len_body);

    if (prop->body && prop->len_body > 0) {
      printf(" value='%.*s'", (int)prop->len_body, prop->body);
    }
    printf("\n");
  }
}

// ==================== FUNCIONES DE LIBERACIÓN ====================

void free_property(Property *prop) {
  if (prop->body)
    free(prop->body);
}

void free_font_char(FontChar *font_char) {
  if (font_char->body)
    free(font_char->body);
}

void free_range(Range *range) {
  if (range->chars) {
    for (uint32_t i = 0; i < (range->end_char - range->start_char) + 1; i++) {
      free_font_char(&range->chars[i]);
    }
    free(range->chars);
  }
}

void free_dat_font_39(DatFont39 *font) {
  if (font->ranges) {
    for (int16_t i = 0; i < font->num_ranges; i++) {
      free_range(&font->ranges[i]);
    }
    free(font->ranges);
  }
}

void free_dat_font(DatFont *font) {
  switch (font->font_size) {
  case 0:
    if (font->body.font39) {
      free_dat_font_39(font->body.font39);
      free(font->body.font39);
    }
    break;
  case 16:
    if (font->body.font16)
      free(font->body.font16);
    break;
  case 8:
    if (font->body.font8)
      free(font->body.font8);
    break;
  }
}

void free_dat_bitmap(DatBitmap *bitmap) {
  if (bitmap->image)
    free(bitmap->image);
  free(bitmap);
}

void free_dat_rle_sprite(DatRleSprite *sprite) {
  if (sprite->image)
    free(sprite->image);
  free(sprite);
}

void free_dat_object(DatObject *object) {
  if (object->properties) {
    // Liberar cada property
    int i = 0;
    while (i < 1000 && property_is_valid(&object->properties[i])) {
      free_property(&object->properties[i]);
      i++;
    }
    free(object->properties);
  }

  if (strncmp(object->type, "BMP ", 4) == 0) {
    if (object->body.bitmap)
      free_dat_bitmap(object->body.bitmap);
  } else if (strncmp(object->type, "FONT", 4) == 0) {
    if (object->body.font)
      free_dat_font(object->body.font);
  } else if (strncmp(object->type, "RLE ", 4) == 0) {
    if (object->body.rle_sprite)
      free_dat_rle_sprite(object->body.rle_sprite);
  }
}

void free_allegro_dat(AllegroDat *dat) {
  if (dat->objects) {
    for (uint32_t i = 0; i < dat->num_objects; i++) {
      free_dat_object(&dat->objects[i]);
    }
    free(dat->objects);
  }
  free(dat);
}

typedef struct {
  BITMAPINFOHEADER infoHeader;
  BITMAPFILEHEADER fileHeader;
  unsigned char *pixelData;
} BMPResult;

uint8_t* convert_bmp_to_allegro_format(BMPResult *bmp) {
    int width = bmp->infoHeader.biWidth;
    int height = abs(bmp->infoHeader.biHeight);
    int bytes_per_pixel = bmp->infoHeader.biBitCount / 8;
    
    // Calcular row stride del BMP (con padding)
    int bmp_row_stride = ((width * bmp->infoHeader.biBitCount + 31) / 32) * 4;
    
    // Calcular tamaño sin padding para Allegro
    int allegro_size = width * height * bytes_per_pixel;
    uint8_t *allegro_data = malloc(allegro_size);
    
    // Convertir fila por fila
    for (int y = 0; y < height; y++) {
        // BMP está invertido (bottom-up), así que leemos desde el final
        int bmp_y = height - 1 - y;
        
        uint8_t *src = bmp->pixelData + (bmp_y * bmp_row_stride);
        uint8_t *dst = allegro_data + (y * width * bytes_per_pixel);
        
        // Copiar y convertir BGR a RGB si es de 24/32 bits
        for (int x = 0; x < width; x++) {
            if (bytes_per_pixel == 3) {
                dst[x*3 + 0] = src[x*3 + 2]; // R
                dst[x*3 + 1] = src[x*3 + 1]; // G
                dst[x*3 + 2] = src[x*3 + 0]; // B
            } else if (bytes_per_pixel == 4) {
                dst[x*4 + 0] = src[x*4 + 2]; // R
                dst[x*4 + 1] = src[x*4 + 1]; // G
                dst[x*4 + 2] = src[x*4 + 0]; // B
                dst[x*4 + 3] = src[x*4 + 3]; // A
            } else {
                // Para 8 bits o menos, copiar directo
                memcpy(dst, src, width * bytes_per_pixel);
                break;
            }
        }
    }
    
    return allegro_data;
}

BMPResult read_bmp(char *filename) {
  FILE *f = fopen(filename, "rb");
  BMPResult res;

  if (!f) {
    perror("Cannot find bitmap\n");
    exit(1);    
  }

  fread(&res.fileHeader, sizeof(BITMAPFILEHEADER), 1, f);
  fread(&res.infoHeader, sizeof(BITMAPINFOHEADER), 1, f);

  // Verificar que sea un BMP
  if (res.fileHeader.bfType != 0x4D42) { // 'BM'
    printf("El archivo no es un bitmap válido.\n");
    fclose(f);
    return res;
  }

  // Calcular tamaño de la imagen si no está en el header
  if (res.infoHeader.biSizeImage == 0)
    res.infoHeader.biSizeImage =
        ((res.infoHeader.biWidth * res.infoHeader.biBitCount + 31) / 32) * 4 *
        abs(res.infoHeader.biHeight);

  // Reservar memoria para los datos
  unsigned char *pixelData = malloc(res.infoHeader.biSizeImage);
  if (!pixelData) {
    printf("Not enough memory available.\n");
    fclose(f);
    return res;
  }

  // Ir al inicio de los datos de imagen
  fseek(f, res.fileHeader.bfOffBits, SEEK_SET);

  // Leer los píxeles
  fread(pixelData, res.infoHeader.biSizeImage, 1, f);

  fclose(f);

  printf("BMP Loaded: %dx%d, %d bits/pixel, size:%d\n",
         res.infoHeader.biWidth, res.infoHeader.biHeight,
         res.infoHeader.biBitCount, res.infoHeader.biSizeImage);

  return res;
}

static uint16_t to_be16(uint16_t x) { return (x >> 8) | (x << 8); }
static uint32_t to_be32(uint32_t x) {
  return ((x >> 24) & 0x000000FF) | ((x >> 8) & 0x0000FF00) |
         ((x << 8) & 0x00FF0000) | ((x << 24) & 0xFF000000);
}

// Escritura Allegro DAT CORREGIDA
int writeallegrodat2(const char *filename, AllegroDat *dat) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    perror("Cannot write dat file\n");
    return 0;
  }
  // Escribir cabecera principal
  uint32_t bepack = to_be32(dat->pack_magic);
  uint32_t bedat = to_be32(dat->dat_magic);
  uint32_t benum = to_be32(dat->num_objects);
  fwrite(&bepack, 4, 1, f);
  fwrite(&bedat, 4, 1, f);
  fwrite(&benum, 4, 1, f);

  // Cada objeto
  for (uint32_t i = 0; i < dat->num_objects; i++) {
    DatObject *obj = &dat->objects[i];
    int32_t belencomp = to_be32(obj->len_compressed);
    int32_t belenuncomp = to_be32(obj->len_uncompressed);

    // Escribir SOLO propiedades válidas
    int propcount = 0;
    while (propcount < 100) {
      const Property *prop = &obj->properties[propcount];
      if (!property_is_valid(prop))
        break;
      fwrite(prop->magic, 1, 4, f); // magic
      fwrite(prop->type, 1, 4, f);  // type
      uint32_t belen = to_be32(prop->len_body);
      fwrite(&belen, 4, 1, f); // lenbody
      if (prop->len_body > 0 && prop->body)
        fwrite(prop->body, 1, prop->len_body, f); // body
      propcount++;
    }

    // Escribir el tipo del objeto JUSTO DESPUÉS de las properties
    fwrite(obj->type, 1, 4, f);

    // Escribir longitudes
    fwrite(&belencomp, 4, 1, f);
    fwrite(&belenuncomp, 4, 1, f);

    // Ejemplo: escribir cuerpo bitmap
    if (memcmp(obj->type, "BMP ", 4) == 0) {
      DatBitmap *bmp = obj->body.bitmap;
      int16_t bebpp = to_be16(bmp->bits_per_pixel);
      uint16_t bew = to_be16(bmp->width);
      uint16_t beh = to_be16(bmp->height);
      fwrite(&bebpp, 2, 1, f);
      fwrite(&bew, 2, 1, f);
      fwrite(&beh, 2, 1, f);
      fwrite(bmp->image, 1, obj->len_uncompressed, f);
    }
    // TODO (Idem para FONT, RLE...)
  }
  fclose(f);
  return 1;
}

int count_properties(Property *properties) {
  int count = 0;
  while (count < 1000) { // Límite de seguridad
    Property *prop = &properties[count];
    if (prop->magic[0] == 0 && prop->magic[1] == 0 && prop->magic[2] == 0 &&
        prop->magic[3] == 0) {
      break;
    }
    count++;
  }
  return count;
}

// ==================== EJEMPLO DE USO ====================
void read_dat_file(char *filename) {
  FILE *file = fopen(filename, "rb");
  if(file == NULL) {
    printf("Cannot read file '%s'.\n", filename);
    exit(1);
  }
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  uint8_t *buffer = (uint8_t *)malloc(file_size);
  fread(buffer, 1, file_size, file);
  fclose(file);

  // Deserializar
  AllegroDat *dat = deserialize_allegro_dat(buffer, file_size);

  // Usar los datos...
  printf("Número de objetos: %u\n", dat->num_objects);

  for (int i = 0; i < dat->num_objects; i++) {
    DatObject obj = dat->objects[i];

    int num_properties = count_properties(dat->objects[i].properties);
    printf("num_props:'%d', type:'%s'\n", num_properties, obj.type);
    for (int j = 0; j < num_properties; j++) {
      Property prop = dat->objects[i].properties[j];

      printf("type:'%s', body:'%s', magic:'%s'\n", prop.type, prop.body,
             prop.magic);
    }
  }
  // Liberar memoria
  free_allegro_dat(dat);
  free(buffer);
}

int main(int argc, char *argv[]) {
  printf("**** Allegro Dat drop-in replacement ****\n");
  if (argc > 1 && strcmp(argv[1], "read") == 0 && argc == 3) {
    read_dat_file(argv[2]);
  } else if (argc > 1 && strcmp(argv[1], "create") == 0) {
    // argv[0] = ejecutable, argv[1] = "crear", argv[2] = salida.dat
    // argv[3] en adelante = archivos a incluir
    if (argc < 3) {
      printf("Wrong parameters, use:\n./dat create file.dat file1.bmp file2.bmp...\n");
      exit(1);
    }
    int num_files = argc - 3;
    const char **input_files = (const char **)&argv[3];
    const char * dat_file =  argv[2];
    printf("Resulting dat filename will be: '%s'\n", dat_file);

    printf("* Including files (%d)...\n", num_files);
    for (int i = 0; i < num_files; i++) {
      printf("%d:%s\n", i, input_files[i]);
    }
    // write case: TODO read from input the files
    AllegroDat *dat = (AllegroDat *)malloc(sizeof(AllegroDat));
    dat->dat_magic = 0x414C4C2E;
    dat->pack_magic = 0x736C682E;
    dat->num_objects = 1;
    dat->objects = malloc(sizeof(DatObject)); // TODO one per object
    // dat->objects[0].body = (void*) NULL;
    memcpy(dat->objects[0].type, "BMP ", 4);

    // 3 + 1 (la ultima invalida)
    dat->objects[0].properties = malloc(sizeof(Property) * 4);
    for (int i = 0; i < 3; i++)
      memcpy(dat->objects[0].properties[i].magic, "prop", 4);

    memcpy(dat->objects[0].properties[0].type, "DATE", 4);

    dat->objects[0].properties[0].body = "10-23-2025, 8:05";
    dat->objects[0].properties[0].len_body = strlen("10-23-2025, 8:05");

    memcpy(dat->objects[0].properties[1].type, "NAME", 4);
    dat->objects[0].properties[1].body = "COCHE_SPRITESHEET_BMP";
    dat->objects[0].properties[1].len_body = strlen("COCHE_SPRITESHEET_BMP");

    memcpy(dat->objects[0].properties[2].type, "ORIG", 4);
    dat->objects[0].properties[2].body = "/foo";
    dat->objects[0].properties[2].len_body = strlen("/foo");
    Property *end = &dat->objects[0].properties[3];

    BMPResult sprite = read_bmp("sprite1.bmp");

    DatBitmap *bitmap = malloc(sizeof(DatBitmap));
    bitmap->bits_per_pixel = sprite.infoHeader.biBitCount;
    bitmap->height = sprite.infoHeader.biHeight;
    bitmap->width = sprite.infoHeader.biWidth;
    bitmap->image = convert_bmp_to_allegro_format(&sprite);
    dat->objects[0].len_compressed = sprite.infoHeader.biSizeImage;
    dat->objects[0].len_uncompressed = sprite.infoHeader.biSizeImage;
    dat->objects[0].body.bitmap = bitmap;

    if (writeallegrodat2(dat_file, dat)) {
      printf("Dat '%s' wrote successfully!\n", dat_file);
    } else {
      printf("Dat '%s', error writing!\n", dat_file);
      exit(1);
    }
  } else {
    printf("Usage: ./dat read file.dat\n./dat create file1.bmp file2.bmp...\n\nCreates a dat file from Allegro 4, effectively merging all static files into a one blob.\n");
  }

  return 0;
}
