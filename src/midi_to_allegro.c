/* midi_to_allegro.c
 *
 * Converts a standard SMF (.mid) file to the Allegro 4 internal MIDI
 * format used inside .dat datafiles.
 *
 * Allegro 4 internal MIDI body layout (all multi-byte values big-endian):
 *
 *   s16  divisions                   (ticks per quarter-note)
 *   Then exactly ALLEGRO_MIDI_TRACKS (32) track slots:
 *     s32  track_len                 (0 = empty / unused track)
 *     u8   track_data[track_len]     (raw SMF track chunk payload,
 *                                     i.e. without the "MTrk" + length header)
 *
 * Reference: Allegro 4 source  allegro/src/midi.c  load_midi_datafile()
 *            and allegro/tools/grabber – MIDI import.
 */

#include "midi_to_allegro.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Portable big-endian read helpers (no alignment required)            */
/* ------------------------------------------------------------------ */

static unsigned short read_be16(const unsigned char *p)
{
    return (unsigned short)(((unsigned)p[0] << 8) | p[1]);
}

static unsigned int read_be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24)
         | ((unsigned int)p[1] << 16)
         | ((unsigned int)p[2] <<  8)
         |  (unsigned int)p[3];
}

static void write_be16(unsigned char *p, unsigned short v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)(v & 0xFF);
}

static void write_be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)((v >> 24) & 0xFF);
    p[1] = (unsigned char)((v >> 16) & 0xFF);
    p[2] = (unsigned char)((v >>  8) & 0xFF);
    p[3] = (unsigned char)( v        & 0xFF);
}

/* ------------------------------------------------------------------ */
/* Main conversion                                                      */
/* ------------------------------------------------------------------ */

int mid_to_allegro_dat(const unsigned char *mid_data,
                       unsigned int         mid_size,
                       unsigned char      **out_buf,
                       unsigned int        *out_size)
{
    unsigned int   pos = 0;
    unsigned short smf_format, smf_ntracks, smf_divisions;
    unsigned int   i;

    /* Track payload pointers and lengths (up to ALLEGRO_MIDI_TRACKS) */
    const unsigned char *track_data[ALLEGRO_MIDI_TRACKS];
    unsigned int         track_len [ALLEGRO_MIDI_TRACKS];

    unsigned int total_track_bytes;
    unsigned int body_size;
    unsigned char *buf;
    unsigned char *wp;

    memset(track_data, 0, sizeof(track_data));
    memset(track_len,  0, sizeof(track_len));

    /* --- Parse MThd ------------------------------------------------ */
    if (mid_size < 14)                           return 0;  /* too small */
    if (memcmp(mid_data, "MThd", 4) != 0)        return 0;  /* not SMF   */

    {
        unsigned int mthd_len = read_be32(mid_data + 4);
        if (mthd_len < 6 || 8 + mthd_len > mid_size) return 0;

        smf_format    = read_be16(mid_data + 8);
        smf_ntracks   = read_be16(mid_data + 10);
        smf_divisions = read_be16(mid_data + 12);

        /* SMPTE timecode (top bit set) is not supported by Allegro 4 */
        if (smf_divisions & 0x8000) return 0;
        /* SMF format 2 (multi-song) not supported */
        if (smf_format == 2)        return 0;

        pos = 8 + mthd_len;   /* skip to first chunk after MThd */
    }

    /* --- Parse MTrk chunks ----------------------------------------- */
    {
        unsigned int stored = 0;

        while (pos + 8 <= mid_size && stored < (unsigned int)ALLEGRO_MIDI_TRACKS)
        {
            unsigned int chunk_len;

            /* Only process MTrk chunks; skip unknown chunk types */
            chunk_len = read_be32(mid_data + pos + 4);
            pos += 8;

            if (memcmp(mid_data + pos - 8, "MTrk", 4) == 0)
            {
                if (pos + chunk_len > mid_size) return 0; /* truncated */

                track_data[stored] = mid_data + pos;
                track_len [stored] = chunk_len;
                stored++;
            }

            pos += chunk_len;
        }

        /*
         * For format 0 the single track contains data for all channels.
         * Allegro 4 expects it in track slot 0 just as-is.
         * No special splitting needed – Allegro handles it at runtime.
         *
         * If smf_ntracks says more tracks than we found, we just leave the
         * remaining slots at 0 (empty).
         */
        (void)smf_ntracks; /* we use actual chunks found, not the header count */
    }

    /* --- Build Allegro body buffer ---------------------------------- */
    /*
     * Layout:
     *   2  bytes  s16 divisions
     *   ALLEGRO_MIDI_TRACKS * 4  bytes  (s32 len per track)
     *   sum of all non-zero track_len values
     */
    total_track_bytes = 0;
    for (i = 0; i < (unsigned int)ALLEGRO_MIDI_TRACKS; i++)
        total_track_bytes += track_len[i];

    body_size = 2u + (unsigned int)(ALLEGRO_MIDI_TRACKS * 4) + total_track_bytes;

    buf = (unsigned char *)malloc(body_size);
    if (!buf) return 0;

    wp = buf;

    /* divisions */
    write_be16(wp, smf_divisions);
    wp += 2;

    /* 32 track slots */
    for (i = 0; i < (unsigned int)ALLEGRO_MIDI_TRACKS; i++)
    {
        write_be32(wp, track_len[i]);
        wp += 4;
        if (track_len[i] > 0)
        {
            memcpy(wp, track_data[i], track_len[i]);
            wp += track_len[i];
        }
    }

    *out_buf  = buf;
    *out_size = body_size;
    return 1;
}
