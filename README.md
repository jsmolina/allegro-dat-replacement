# allegro-dat-replacement
Allegro dat file generator in pure C and structs, WIP.

Mostly working!

Dat format definition obtained from: https://formats.kaitai.io/allegro_dat/index.html


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
```
