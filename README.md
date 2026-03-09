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
- the format is rigid and difficult to modify
- integrating modern asset pipelines is complicated

## What this repository does

This repo implements **a replacement or alternative for the Allegro `.dat` system**. In practice, it:

- Allows **loading game resources without using the original `.dat` format**.
- Replaces the old mechanism with **more modern file or asset structures** (for example, individual files or a different packaging system).
- Provides **code that mimics Allegro’s datafile access API**, so existing game code can keep working while using a different resource backend.

In other words:

> It acts as a **drop-in replacement for Allegro’s `.dat` resource system**, allowing games to use another asset format or loading system.

## What it’s useful for

Mainly for:

- **modernizing old Allegro 4 projects**
- **removing the dependency on the `.dat` format**
- making asset management easier (sprites, audio, etc.)
- integrating more modern build pipelines
