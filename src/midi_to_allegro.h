/* midi_to_allegro.h
 * Converts a standard MIDI file (.mid) into the Allegro 4 internal MIDI
 * format as stored inside a .dat datafile.
 *
 * Allegro 4 internal format (big-endian on disk):
 *
 *   [0 : 2]   s16  divisions   (ticks per quarter-note, from MThd)
 *   [2 : ...]  32 track slots (MIDI_TRACKS = 32), each:
 *                s32  len      (byte length of track data, or 0 if empty)
 *                u8   data[len]
 *
 * Reference: allegro 4 source  src/midi.c  load_midi_datafile()
 *            and               src/grabber/grabber.c  (import MIDI)
 */
#ifndef MIDI_TO_ALLEGRO_H
#define MIDI_TO_ALLEGRO_H

#include <stddef.h>

/*
 * mid_to_allegro_dat
 *
 * Reads a standard SMF (.mid) file from raw bytes [mid_data, mid_size),
 * converts it to the Allegro-4 internal MIDI format, and writes the result
 * into a newly-allocated buffer returned via *out_buf / *out_size.
 *
 * Supports SMF format 0 (single track) and format 1 (multi-track).
 * Format 2 is not supported (returns 0).
 * Up to ALLEGRO_MIDI_TRACKS (32) tracks are stored; extras are silently dropped.
 *
 * Returns 1 on success, 0 on error.
 * Caller must free(*out_buf).
 */
int mid_to_allegro_dat(const unsigned char *mid_data,
                       unsigned int         mid_size,
                       unsigned char      **out_buf,
                       unsigned int        *out_size);

#define ALLEGRO_MIDI_TRACKS 32

#endif /* MIDI_TO_ALLEGRO_H */
