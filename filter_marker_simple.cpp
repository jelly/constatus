// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter_marker_simple.h"
#include "log.h"

filter_marker_simple::filter_marker_simple(const sm_mode_t modeIn, const uint8_t *const pixel_select_bitmap, meta *const m, const int noise_level) : mode(modeIn), psb(pixel_select_bitmap), m(m), noise_level(noise_level)
{
}

filter_marker_simple::~filter_marker_simple()
{
	free((void *)psb);
}

void filter_marker_simple::updatePixel(uint8_t *const out, const int x, const int y, const int w)
{
	const int o = y * w * 3 + x * 3;

	switch(mode) {
		case m_red:
			out[o + 0] = 255;
			break;

		case m_red_invert:
			out[o + 0] ^= 255;
			break;

		case m_invert:
			out[o + 0] ^= 255;
			out[o + 1] ^= 255;
			out[o + 2] ^= 255;
			break;
	}
}

void filter_marker_simple::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (!prev)
		return;

	bool *diffs = new bool[w * h];

	const int nl3 = noise_level * 3;

	if (psb) {
		for(int i=0; i<w*h; i++) {
			if (!psb[i])
				continue;

			int i3 = i * 3;

			int lc = in_out[i3 + 0] + in_out[i3 + 1] + in_out[i3 + 2];
			int lp = prev[i3 + 0] + prev[i3 + 1] + prev[i3 + 2];

			diffs[i] = abs(lc - lp) >= nl3;
		}
	}
	else {
		for(int i=0; i<w*h*3; i += 3) {
			int lc = in_out[i + 0] + in_out[i + 1] + in_out[i + 2];
			int lp = prev[i + 0] + prev[i + 1] + prev[i + 2];

			diffs[i / 3] = abs(lc - lp) >= nl3;
		}
	}

	int cx = 0, cy = 0, cn = 0;

	for(int o=0; o<w*h; o++) {
		if (diffs[o]) {
			int x = o % w;
			int y = o / w;

			cx += x;
			cy += y;
			cn++;
		}
	}

	if (!cn) {
		delete [] diffs;
		return;
	}

	cx /= cn;
	cy /= cn;

	m -> setInt("$motion-center-x$", std::pair<uint64_t, int>(ts, cx));
	m -> setInt("$motion-center-y$", std::pair<uint64_t, int>(ts, cy));

	int xdist = 0, ydist = 0;

	for(int o=0; o<w*h; o++) {
		if (diffs[o]) {
			int x = o % w;
			int y = o / w;

			xdist += abs(x - cx);
			ydist += abs(y - cy);
		}
	}

	delete [] diffs;

	xdist /= cn;
	ydist /= cn;

	m -> setInt("$motion-width$", std::pair<uint64_t, int>(ts, xdist));
	m -> setInt("$motion-height$", std::pair<uint64_t, int>(ts, ydist));

	int xmin = std::max(0, cx - xdist), xmax = std::min(w - 1, cx + xdist);
	int ymin = std::max(0, cy - ydist), ymax = std::min(h - 1, cy + ydist);

	log(LL_DEBUG_VERBOSE, "%d,%d - %d,%d", xmin, ymin, xmax, ymax);

	for(int y=ymin; y<ymax; y++) {
		updatePixel(in_out, xmin, y, w);
		updatePixel(in_out, xmax, y, w);
	}

	for(int x=xmin; x<xmax; x++) {
		updatePixel(in_out, x, ymin, w);
		updatePixel(in_out, x, ymax, w);
	}
}
