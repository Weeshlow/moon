/*
 * brush.cpp: Brushes
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */
#define __STDC_CONSTANT_MACROS
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <malloc.h>
#include <glib.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "runtime.h"

void
SolidColorBrush::SetupBrush (cairo_t *target)
{
	cairo_set_source_rgba (target, color.r, color.g, color.b, color.a);
}

typedef struct {
	const char *name;
	const long color;
} named_brush_t;

named_brush_t named_brushes [] = {
	{ "red", 0xFFFF0000 },
	{ "green", 0xFF00FF00 },
	{ "blue", 0xFF0000FF },
	{ "black", 0xFF000000 },
	{ "white", 0xFFFFFFFF },
	{ NULL, 0 }
};

/**
 * see: http://msdn2.microsoft.com/en-us/library/system.windows.media.solidcolorbrush.aspx
 */
SolidColorBrush *
solid_brush_from_str (const char *name)
{
	if (!name)
		return NULL;

	if (name [0] == '#') {
		char a [3] = "FF";
		char r [3] = "FF";
		char g [3] = "FF";
		char b [3] = "FF";

		switch (strlen (name + 1)) {
		case 3:
			// rgb
			r [0] = '0'; r [1] = name [1];
			g [0] = '0'; g [1] = name [2];
			b [0] = '0'; b [1] = name [3];
			break;
		case 4:
			// argb
			a [0] = '0'; a [1] = name [1];
			r [0] = '0'; r [1] = name [2];
			g [0] = '0'; g [1] = name [3];
			b [0] = '0'; b [1] = name [4];
			break;
		case 6:
			// rrggbb
			r [0] = name [1]; r [1] = name [2];
			g [0] = name [3]; g [1] = name [4];
			b [0] = name [5]; b [1] = name [6];
			break;
		case 8:
			// rrggbb
			a [0] = name [1]; a [1] = name [2];
			r [0] = name [3]; r [1] = name [4];
			g [0] = name [5]; g [1] = name [6];
			b [0] = name [7]; b [1] = name [8];
			break;			
		}

		return new SolidColorBrush (Color (strtol (r, NULL, 16) / 255.0F,
							    strtol (g, NULL, 16) / 255.0F,
							    strtol (b, NULL, 16) / 255.0F,
							    strtol (a, NULL, 16) / 255.0F));
	}

	if (name [0] == 's' && name [1] == 'c' && name [2] == '#') {
		/* TODO */
	}

	for (int i = 0; named_brushes [i].name; i++) {
		if (!g_strcasecmp (named_brushes [i].name, name)) {
			Color c = Color (((named_brushes [i].color >> 16) & 0x00FF) / 255.0F,
					 ((named_brushes [i].color >> 8) & 0x0000FF) / 255.0F,
					 ((named_brushes [i].color) & 0x000000FF) / 255.0F,
					 ((named_brushes [i].color >> 24) & 0xFF) / 255.0F);
			return new SolidColorBrush (c);
		}
	}
	return NULL;
}
