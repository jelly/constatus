// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <string.h>
#include <time.h>

#include "filter_add_text.h"
#include "error.h"

constexpr unsigned char font[128][8][8] = {
	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 255, 255, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 255, 0 },
		{ 255, 0, 255, 0, 0, 255, 0, 255 },
		{ 255, 0, 0, 0, 0, 0, 0, 255 },
		{ 255, 0, 255, 0, 0, 255, 0, 255 },
		{ 255, 0, 0, 255, 255, 0, 0, 255 },
		{ 0, 255, 0, 0, 0, 0, 255, 0 },
		{ 0, 0, 255, 255, 255, 255, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 255, 255, 0, 0 },
		{ 0, 255, 255, 255, 255, 255, 255, 0 },
		{ 255, 255, 0, 255, 255, 0, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 0, 255, 255, 0, 255, 255 },
		{ 0, 255, 255, 0, 0, 255, 255, 0 },
		{ 0, 0, 255, 255, 255, 255, 0, 0 }
	},

	{
		{ 0, 255, 255, 0, 255, 255, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 255, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 0, 255, 0, 255, 0, 255, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 0, 0, 255, 255, 255 },
		{ 255, 255, 255, 0, 0, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 }
	},

	{
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 255, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 255, 0 },
		{ 255, 0, 0, 0, 0, 0, 255, 0 },
		{ 255, 0, 0, 0, 0, 0, 255, 0 },
		{ 0, 255, 0, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 0, 0, 0, 255, 255, 255 },
		{ 255, 0, 255, 255, 255, 0, 255, 255 },
		{ 0, 255, 255, 255, 255, 255, 0, 255 },
		{ 0, 255, 255, 255, 255, 255, 0, 255 },
		{ 0, 255, 255, 255, 255, 255, 0, 255 },
		{ 255, 0, 255, 255, 255, 0, 255, 255 },
		{ 255, 255, 0, 0, 0, 255, 255, 255 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 }
	},

	{
		{ 0, 0, 0, 0, 255, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 255, 255 },
		{ 0, 0, 0, 0, 0, 255, 0, 255 },
		{ 0, 255, 255, 255, 255, 0, 0, 255 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 255, 0, 0 },
		{ 0, 255, 0, 0, 0, 255, 0, 0 },
		{ 0, 255, 0, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 255, 255, 255, 0, 0, 255, 0, 0 },
		{ 255, 255, 0, 255, 255, 255, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 255, 0, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 255, 255, 255, 0, 255, 255, 255, 0 },
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 255, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 255, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 0, 0, 0, 255 },
		{ 0, 255, 0, 0, 0, 0, 255, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 255, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 255, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 255 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 255 },
		{ 0, 0, 0, 0, 0, 0, 255, 0 },
		{ 0, 0, 0, 0, 0, 255, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 255, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 255, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 255 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 255, 255, 255 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 }
	},

	{
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 255, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 255, 255, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 255, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 255, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 0, 255, 0, 0, 0 },
		{ 255, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 255, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 255, 0, 0, 0 },
		{ 0, 255, 255, 255, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 255, 255, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 255, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 0, 0, 255, 0, 0, 0, 0, 0 },
		{ 255, 255, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	},

	{
		{ 0, 255, 0, 0, 0, 0, 0, 0 },
		{ 255, 0, 255, 0, 255, 0, 0, 0 },
		{ 0, 0, 0, 255, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0 }
	}

};

filter_add_text::filter_add_text(const std::string & whatIn, const text_pos_t tpIn) : what(whatIn), tp(tpIn)
{
}

filter_add_text::~filter_add_text()
{
}

void add_text(unsigned char *const img, const int width, const int height, const char *const text, const int xpos, const int ypos)
{
	int cx = 0, cy = 0;
	const int len = strlen(text);

	for(int loop=0; loop<len; loop++)
	{
		if (text[loop] == '\\' && text[loop + 1] == 'n') {
			cy++;
			cx = 0;
			loop++; // ignore 'n'
			continue;
		}

		for(int y=0; y<8; y++)
		{
			for(int x=0; x<8; x++)
			{
				int cur_char = text[loop];
				int realx = xpos + x + 8 * cx, realy = ypos + y + cy * 9;
				int offset = (realy * width * 3) + (realx * 3);

				if (realx >= width || realx < 0 || realy >= height || realy < 0)
					break;

				if (cur_char < 0)
					cur_char = 32;

				img[offset + 0] = font[cur_char][y][x];
				img[offset + 1] = font[cur_char][y][x];
				img[offset + 2] = font[cur_char][y][x];
			}
		}

		cx++;
	}
}

void print_timestamp(unsigned char *const img, const int width, const int height, const char *const text, const text_pos_t n_pos, const uint64_t ts)
{
	int x = 0, y = 0;

	time_t now = (time_t)(ts / 1000 / 1000);
	struct tm ptm;
	localtime_r(&now, &ptm);

	int len = strlen(text);
	int bytes = len + 4096;

	char *text_out = (char *)malloc(bytes);
	if (!text_out)
		error_exit(true, "out of memory while allocating %d bytes", bytes);

	int new_len = strftime(text_out, bytes, text, &ptm);

	int n_lines = 1, cur_ll = 0, max_ll = 0;
	for(int i=0; i<new_len; i++) {
		if (text_out[i] == '\\' && text_out[i + 1] == 'n') {
			if (cur_ll > max_ll)
				max_ll = cur_ll;

			n_lines++;
			cur_ll = 0;
		}
		else {
			cur_ll++;
		}
	}
	if (cur_ll > max_ll)
		max_ll = cur_ll;

	if (n_pos == upper_left || n_pos == upper_center || n_pos == upper_right)
		y = 1;
	else if (n_pos == center_left || n_pos == center_center || n_pos == center_right)
		y = (height / 2) - 4;
	else if (n_pos == lower_left || n_pos == lower_center || n_pos == lower_right)
		y = height - 9 * n_lines;

	if (n_pos == upper_left || n_pos == center_left || n_pos == lower_left)
	{
		x = 1;
	}
	else if (n_pos == upper_center || n_pos == center_center || n_pos == lower_center)
	{
		x = (width / 2) - (max_ll / 2) * 8;
	}
	else if (n_pos == upper_right || n_pos == center_right || n_pos == lower_right)
	{
		x = width - max_ll * 8;
	}

	add_text(img, width, height, text_out, x, y);

	free(text_out);
}

void filter_add_text::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	print_timestamp(in_out, w, h, what.c_str(), tp, ts);
}
