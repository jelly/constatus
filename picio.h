// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdio.h>
void write_PNG_file(FILE *fh, int ncols, int nrows, unsigned char *pixels);
void write_JPEG_file(FILE *fh, int ncols, int nrows, int quality, const unsigned char *pixels_out);
void write_JPEG_memory(const int ncols, const int nrows, const int quality, const unsigned char *const pixels, char **out, size_t *out_len);
void read_JPEG_memory(unsigned char *in, int n_bytes_in, int *ncols, int *nrows, unsigned char **pixels);
