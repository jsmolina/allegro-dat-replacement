# allegro-dat-replacement
Allegro dat file generator in ANSI C, mostly complete.

Mostly working!

Dat format definition obtained from: https://formats.kaitai.io/allegro_dat/index.html and https://liballeg.org/stabledocs/en/datafile.html
+ Help from Claude

Compiling: 
`Make all`

Executing: 
```bash

Usage:
  dat <archivo.dat> -l
  dat <archivo.dat> [-t TYPE] -a <file> [[-t TYPE] -a <file> ...] [--h header.h]

Options:
  -l            List contents of an existing .dat file
  -a <file>     Add file (type auto-detected from extension)
  -t TYPE       Force type for the next -a only, then resets
  --h <file>    Generate C header with #define indices

Auto-detected types by extension:
  .bmp -> BMP | .rle -> RLE | .mid/.midi -> MIDI
  .wav -> SAMP | .fli/.flc -> FLIC | .act/.pal -> PAL
  (other) -> DATA

Explicit -t types: BMP PAL RLE FONT8 FONT16 FONT MIDI SAMP WAV FLIC DATA CMP XCMP PAT
```
--h will create a header file with the references to the elements.

## What problem it solves

In **Allegro 4**, it was common to use **`.dat` files** as containers for game resources (sprites, sounds, maps, etc.). These files were generated using the `dat` tool included with the library. This system had several limitations:

- it depended on an old tool (`dat`)
- this tool doesn't compile easily.

## What this repository does

This repo implements **a replacement or alternative for the Allegro `.dat` system**. In practice, it:

- Allows **storing game resources without using the original `.dat` format**.
- Provides simpler ansi-c **structures**

In other words:

> It acts as a **drop-in replacement for Allegro’s `.dat` creation utilies** generating allegro 4 compatible DAT files.

## What it’s useful for

Mainly for:

- **modernizing old Allegro 4 projects**
- **removing the dependency on the allegro boilerplate for generating dats**
- making asset management easier (sprites, audio, etc.)
- integrating more modern build pipelines

## Contributors
@jsmolina
@warrior-rockk
