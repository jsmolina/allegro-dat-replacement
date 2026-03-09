/* wav_to_allegro.c
 *
 * Convierte un fichero WAV RIFF/PCM al formato interno SAMP de Allegro 4
 * para su almacenamiento en un fichero .dat.
 *
 * Formato SAMP body (big-endian en disco):
 *
 *   [0:2]  s16  bits    signo = flag estéreo, |bits| = profundidad de bit
 *                       -8  = 8-bit unsigned estéreo
 *                       +8  = 8-bit unsigned mono
 *                       -16 = 16-bit unsigned estéreo   (bytes BE por muestra)
 *                       +16 = 16-bit unsigned mono      (bytes BE por muestra)
 *   [2:4]  u16  freq    frecuencia de muestreo en Hz
 *   [4:8]  s32  len     número de frames (= muestras por canal)
 *   [8:..] u8[] pcm     datos PCM entrelazados
 *                       8-bit:  verbatim del chunk 'data' del WAV
 *                       16-bit: muestras convertidas de LE a BE (par de bytes invertido)
 *
 * Referencia: Allegro 4  allegro/src/sound/digi.c  load_sample_datafile()
 *             verificado byte a byte contra small2.dat de referencia.
 */

#include "wav_to_allegro.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers de lectura (no requieren alineación)                        */
/* ------------------------------------------------------------------ */

static unsigned short read_le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned)p[1] << 8));
}

static unsigned int read_le32(const unsigned char *p)
{
    return (unsigned int)p[0]
         | ((unsigned int)p[1] <<  8)
         | ((unsigned int)p[2] << 16)
         | ((unsigned int)p[3] << 24);
}

static void write_be16_signed(unsigned char *p, short v)
{
    p[0] = (unsigned char)((unsigned short)v >> 8);
    p[1] = (unsigned char)((unsigned short)v & 0xFF);
}

static void write_be16_unsigned(unsigned char *p, unsigned short v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)(v & 0xFF);
}

static void write_be32_signed(unsigned char *p, int v)
{
    unsigned int u = (unsigned int)v;
    p[0] = (unsigned char)((u >> 24) & 0xFF);
    p[1] = (unsigned char)((u >> 16) & 0xFF);
    p[2] = (unsigned char)((u >>  8) & 0xFF);
    p[3] = (unsigned char)( u        & 0xFF);
}

/* ------------------------------------------------------------------ */
/* Función principal                                                    */
/* ------------------------------------------------------------------ */

int wav_to_allegro_samp(const unsigned char *wav_data,
                        unsigned int         wav_size,
                        unsigned char      **out_buf,
                        unsigned int        *out_size)
{
    unsigned int   pos;
    unsigned short fmt_audio_format;
    unsigned short fmt_channels;
    unsigned int   fmt_sample_rate;
    unsigned short fmt_bits;
    unsigned int   pcm_offset;
    unsigned int   pcm_size;

    unsigned int   num_frames;
    int            alg_bits;       /* campo 'bits' de Allegro, con signo */
    unsigned int   body_size;
    unsigned char *buf;
    unsigned char *wp;

    /* ---- Validar RIFF/WAVE ---------------------------------------- */
    if (wav_size < 12)                          return 0;
    if (memcmp(wav_data,     "RIFF", 4) != 0)  return 0;
    if (memcmp(wav_data + 8, "WAVE", 4) != 0)  return 0;

    fmt_audio_format = 0;
    fmt_channels     = 0;
    fmt_sample_rate  = 0;
    fmt_bits         = 0;
    pcm_offset       = 0;
    pcm_size         = 0;

    /* ---- Recorrer chunks ------------------------------------------ */
    pos = 12;
    while (pos + 8 <= wav_size)
    {
        unsigned int chunk_size = read_le32(wav_data + pos + 4);

        if (memcmp(wav_data + pos, "fmt ", 4) == 0)
        {
            if (chunk_size < 16 || pos + 8 + chunk_size > wav_size) return 0;
            fmt_audio_format = read_le16(wav_data + pos +  8);
            fmt_channels     = read_le16(wav_data + pos + 10);
            fmt_sample_rate  = read_le32(wav_data + pos + 12);
            fmt_bits         = read_le16(wav_data + pos + 22);
        }
        else if (memcmp(wav_data + pos, "data", 4) == 0)
        {
            pcm_offset = pos + 8;
            pcm_size   = chunk_size;
            /* Limitar al tamaño real del fichero */
            if (pcm_offset + pcm_size > wav_size)
                pcm_size = wav_size - pcm_offset;
        }

        /* Avanzar al siguiente chunk (tamaño par por especificación RIFF) */
        pos += 8 + chunk_size + (chunk_size & 1u);
    }

    /* ---- Validaciones -------------------------------------------- */
    if (fmt_audio_format != 1)  return 0;  /* solo PCM */
    if (pcm_offset == 0)        return 0;  /* no hay chunk data */
    if (fmt_channels == 0)      return 0;
    if (fmt_bits != 8 && fmt_bits != 16) return 0;
    if (fmt_sample_rate == 0 || fmt_sample_rate > 65535u) return 0;

    /* ---- Calcular campos Allegro ---------------------------------- */
    /*
     * En Allegro 4 el campo 'bits' codifica tanto la profundidad como el
     * número de canales:
     *   positivo = mono
     *   negativo = estéreo
     * El valor absoluto es la profundidad en bits (8 ó 16).
     */
    alg_bits = (fmt_channels >= 2) ? -(int)fmt_bits : (int)fmt_bits;

    /*
     * 'len' = número de frames (muestras por canal).
     * frames = pcm_bytes / (channels * bytes_per_sample)
     */
    num_frames = pcm_size / (fmt_channels * (fmt_bits / 8u));

    /* ---- Reservar buffer body ------------------------------------ */
    body_size = 8u + pcm_size;
    buf = (unsigned char *)malloc(body_size);
    if (!buf) return 0;

    wp = buf;

    /* bits (s16 BE) */
    write_be16_signed(wp, (short)alg_bits);
    wp += 2;

    /* freq (u16 BE) */
    write_be16_unsigned(wp, (unsigned short)fmt_sample_rate);
    wp += 2;

    /* len (s32 BE) */
    write_be32_signed(wp, (int)num_frames);
    wp += 4;

    /* PCM data ---------------------------------------------------- */
    if (fmt_bits == 8)
    {
        /* 8-bit PCM: verbatim */
        memcpy(wp, wav_data + pcm_offset, pcm_size);
    }
    else
    {
        /*
         * 16-bit PCM: WAV usa little-endian por muestra.
         * Allegro almacena big-endian. Invertir cada par de bytes.
         */
        unsigned int i;
        const unsigned char *src = wav_data + pcm_offset;
        for (i = 0; i + 1 < pcm_size; i += 2)
        {
            wp[i]     = src[i + 1];
            wp[i + 1] = src[i];
        }
        /* Si pcm_size es impar (no debería ocurrir en 16-bit) copia el byte suelto */
        if (pcm_size & 1u) wp[pcm_size - 1] = src[pcm_size - 1];
    }

    *out_buf  = buf;
    *out_size = body_size;
    return 1;
}
