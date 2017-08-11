// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <string.h>
#include <time.h>
#include <cairo/cairo.h>

#include "filter_add_scaled_text.h"
#include "error.h"

cairo_surface_t * rgb_to_cairo(const uint8_t *const in, const int w, const int h)
{
	size_t n = w * h;
	uint32_t *temp = (uint32_t *)malloc(n * 4);

	const uint8_t *win = in;
	uint32_t *wout = temp;

	for(size_t i=0; i<n; i++) {
		uint8_t r = *win++;
		uint8_t g = *win++;
		uint8_t b = *win++;
		*wout++ = (255 << 24) | (r << 16) | (g << 8) | b;
	}

	return cairo_image_surface_create_for_data((unsigned char *)temp, CAIRO_FORMAT_RGB24, w, h, 4 * w);
}

void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out)
{
	const unsigned char *data = cairo_image_surface_get_data(cs);
	const uint32_t *in = (const uint32_t *)data;

	size_t n = w * h;
	for(size_t i=0; i<n; i++) {
		uint32_t temp = *in++;
		*out++ = temp >> 16;
		*out++ = temp >> 8;
		*out++ = temp;
	}
}

filter_add_scaled_text::filter_add_scaled_text(const std::string & what, const std::string & font_name, const int x, const int y, const int font_size, const int r, const int g, const int b) : what(what), font_name(font_name), x(x), y(y), font_size(font_size), r(r), g(g), b(b)
{
}

filter_add_scaled_text::~filter_add_scaled_text()
{
}

void filter_add_scaled_text::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
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

	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); 
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, text_out);

	cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
	cairo_move_to(cr, x + 1, y + 1);
	cairo_show_text(cr, text_out);
	///

	cairo_to_rgb(cs, w, h, out);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);

	free(text_out);
}
