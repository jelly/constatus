#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter_marker_simple.h"

filter_marker_simple::filter_marker_simple(const sm_mode_t modeIn) : mode(modeIn)
{
}

filter_marker_simple::~filter_marker_simple()
{
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

void filter_marker_simple::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	memcpy(out, in, w * h * 3);

	if (!prev)
		return;

	bool *diffs = new bool[w * h];

	for(int i=0; i<w*h*3; i += 3) {
		int lc = (in[i + 0] + in[i + 1] + in[i + 2]) / 3;
		int lp = (prev[i + 0] + prev[i + 1] + prev[i + 2]) / 3;

		diffs[i / 3] = abs(lc - lp) >= 32; // FIXME
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

	int xmin = std::max(0, cx - xdist), xmax = std::min(w - 1, cx + xdist);
	int ymin = std::max(0, cy - ydist), ymax = std::min(h - 1, cy + ydist);

	printf("%d,%d - %d,%d\n", xmin, ymin, xmax, ymax);

	for(int y=ymin; y<ymax; y++) {
		updatePixel(out, xmin, y, w);
		updatePixel(out, xmax, y, w);
	}

	for(int x=xmin; x<xmax; x++) {
		updatePixel(out, x, ymin, w);
		updatePixel(out, x, ymax, w);
	}
}
