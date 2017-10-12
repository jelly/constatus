#include <stddef.h>
#include <stdio.h>

#include "selection_mask.h"
#include "error.h"
#include "picio.h"

selection_mask::selection_mask(resize *const r, const std::string & selection_bitmap) : r(r)
{
	cache = pixels = NULL;
	cw = ch = w = h = -1;

	std::string ext = "pbm";
	if (selection_bitmap.size() >= 3)
		ext = selection_bitmap.substr(selection_bitmap.length() - 3);

	FILE *fh = fopen(selection_bitmap.c_str(), "rb");
	if (!fh)
		error_exit(true, "Cannot open file \"%s\"", selection_bitmap.c_str());

	if (ext == "pbm")
		load_PBM_file(fh, &w, &h, &pixels);
	else if (ext == "png") {
		uint8_t *temp = NULL;
		read_PNG_file_rgba(fh, &w, &h, &temp);

		size_t n = w * h;
		pixels = (uint8_t *)malloc(n);

		for(size_t i=0; i<n; i++) {
			uint8_t gray = (temp[i * 4 + 0] + temp[i * 4 + 1] + temp[i * 4 + 2]) / 3;

			pixels[i] = gray >= 128;
		}

		free(temp);
	}
	else {
		error_exit(false, "File type \"%s\" not understood for the selection mask/bitmap", ext.c_str());
	}

	fclose(fh);
}

selection_mask::~selection_mask()
{
	free(cache);
	free(pixels);
}

uint8_t *selection_mask::get_mask(const int rw, const int rh)
{
	if (rw == w && rh == h)
		return pixels;

	if (rw == cw && rh == ch)
		return cache;

	free(cache);

	r -> do_resize(w, h, pixels, rw, rh, &cache);

	cw = rw;
	ch = rh;

	return cache;
}
