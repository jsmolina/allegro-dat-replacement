# Makefile
CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra

SRC=src/memory_free.c src/dat_writer.c src/dat_loader_bmp.c src/dat_loader_pal.c src/dat_loader_font.c src/dat_cli.c

all: dat

dat: $(SRC)
	$(CC) $(CFLAGS) -o dat $(SRC)

clean:
	rm -f dat
