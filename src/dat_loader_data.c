#include <stdio.h>
#include <stdlib.h>
#include "dat_loader_data.h"

int load_file_bytes(const char* p, unsigned char** b, unsigned int* s) {
    FILE* f = fopen(p, "rb");
    if (!f)
        return 0;
    fseek(f, 0, 2);
    long z = ftell(f);
    fseek(f, 0, 0);
    *b = malloc(z);
    if (!*b) {
        fclose(f);
        return 0;
    }
    if (fread(*b, 1, z, f) != z) {
        free(*b);
        fclose(f);
        return 0;
    }
    fclose(f);
    *s = z;
    return 1;
}