// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <string.h>
#include <time.h>
#include <cairo/cairo.h>

#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "error.h"
#include "cairo.h"
#include "utils.h"

filter_add_scaled_text::filter_add_scaled_text(const std::string & what, const std::string & font_name, const int x, const int y, const int font_size, const int r, const int g, const int b) : what(what), font_name(font_name), x(x), y(y), font_size(font_size), r(r), g(g), b(b)
{
}

filter_add_scaled_text::~filter_add_scaled_text()
{
}

void filter_add_scaled_text::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	time_t now = time(NULL);
	struct tm ptm;
	localtime_r(&now, &ptm);

	size_t bytes = what.size() + 4096;

	char *text_out = (char *)malloc(bytes);
	if (!text_out)
		error_exit(true, "out of memory while allocating %d bytes", bytes);

	strftime(text_out, bytes, what.c_str(), &ptm);

	cairo_surface_t *const cs = rgb_to_cairo(in, w, h);
	cairo_t *const cr = cairo_create(cs);

	///
	cairo_select_font_face(cr, font_name.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_size);

	int n_lines = 0, n_cols = 0;
	find_text_dim(text_out, &n_lines, &n_cols);

	std::vector<std::string> *parts = split(text_out, "\\n");

	double R = r / 255.0, G = g / 255.0, B = b / 255.0;

	int work_y = y;
	for(std::string cl : *parts) {
		cairo_set_source_rgb(cr, 1.0 - R, 1.0 - G, 1.0 - B); 
		cairo_move_to(cr, x, work_y);
		cairo_show_text(cr, cl.c_str());

		cairo_set_source_rgb(cr, R, G, B);
		cairo_move_to(cr, x + 1, work_y + 1);

		cairo_show_text(cr, cl.c_str());

		work_y += font_size + 1;
	}
	///

	delete parts;

	cairo_to_rgb(cs, w, h, out);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);

	free(text_out);
}
