# allegro-dat-replacement
Allegro dat file generator in ANSI C, mostly complete.

Mostly working!

Dat format definition obtained from: https://formats.kaitai.io/allegro_dat/index.html and https://liballeg.org/stabledocs/en/datafile.html
+ Help from Claude

Compiling: 
`Make all`

Executing: 
```bash
dat create out.dat
      [--bmp file.bmp]*
      [--pal file.act]*
      [--pal-bmp file.bmp]*
      [--rle file.rle]*
      [--font8-bmp file.bmp]*
      [--font16-bmp file.bmp]*
      [--midi file.midi]*
      [--wav file.wav]*
      [--data file.bin]*
      [--flic file.fli]*

dat list in.dat
```

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
