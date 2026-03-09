/* wav_to_allegro.h
 *
 * Convierte un fichero WAV (RIFF PCM) al formato interno SAMP de Allegro 4
 * tal como se almacena en un fichero .dat.
 *
 * Formato del body SAMP en disco (big-endian):
 *
 *   [0:2]  s16  bits   signo = flag estereo; |bits| = profundidad
 *                      -8  = 8-bit  unsigned estéreo
 *                      +8  = 8-bit  unsigned mono
 *                      -16 = 16-bit unsigned estéreo
 *                      +16 = 16-bit unsigned mono
 *   [2:4]  u16  freq   frecuencia de muestreo (Hz)
 *   [4:8]  s32  len    número de frames (muestras por canal)
 *   [8:..] u8[] pcm    datos PCM entrelazados, verbatim del chunk 'data' del WAV
 *
 * Tamaño total del body = 8 + len * (|bits|/8) * (estéreo ? 2 : 1)
 *
 * Referencia: Allegro 4 src/sound/digi.c  load_sample_datafile()
 *             analizado sobre small2.dat de referencia.
 */
#ifndef WAV_TO_ALLEGRO_H
#define WAV_TO_ALLEGRO_H

/*
 * wav_to_allegro_samp
 *
 * Lee un WAV PCM de los bytes [wav_data, wav_size) y genera el body
 * SAMP de Allegro 4 en un buffer recién reservado devuelto en
 * *out_buf / *out_size.
 *
 * Soporta PCM 8-bit y 16-bit, mono y estéreo.
 * WAV de 16-bit se convierte de little-endian a big-endian muestra a muestra.
 * Otros formatos (float, ADPCM, etc.) devuelven 0.
 *
 * Devuelve 1 si OK, 0 en error. El llamador debe free(*out_buf).
 */
int wav_to_allegro_samp(const unsigned char *wav_data,
                        unsigned int         wav_size,
                        unsigned char      **out_buf,
                        unsigned int        *out_size);

#endif /* WAV_TO_ALLEGRO_H */
