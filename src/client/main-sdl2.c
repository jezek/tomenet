#ifdef USE_SDL2
/* main-sdl2.c
 *
 * SDL2 bindings implementation for the TomeNET game project.
 * Handles window creation, event handling, keyboard input, and graphics/text output using SDL2.
 *
 * Note: Requires SDL2, SDL2_ttf and SDL2_image libraries.
 */

#include "angband.h"

#include "../common/z-util.h"
#include "../common/z-virt.h"
#include "../common/z-form.h"

#ifndef __MAKEDEPEND__
 #include <SDL2/SDL.h>
 #include <SDL2/SDL_ttf.h>
 #include <SDL2/SDL_net.h>
 #include <SDL2/SDL_image.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef REGEX_SEARCH
 #include <regex.h>
#endif

/**** Available Types ****/

/*
 * Notes on Colors:
 *
 *   1) On a monochrome (or "fake-monochrome") display, all colors
 *   will be "cast" to "fg," except for the bg color, which is,
 *   obviously, cast to "bg".
 *
 *   2) Because of the inner functioning of the color allocation
 *   routines, colors may be specified as (a) a typical color name,
 *   (b) a hexadecimal color specification (preceded by a pound sign),
 *   or (c) by strings such as "fg", "bg".
 */

/* Pixel is just shorthand for SDL_Color. */
typedef SDL_Color Pixel;

/* The structures defined below */
typedef struct infowin infowin;
typedef struct infoclr infoclr;
typedef struct infofnt infofnt;

/*
 * A Structure summarizing Window Information.
 *
 *	- The window.
 *	- The drawing surface for the window.
 *
 *	- The window identifier.
 *	- Just used for padding.
 *
 *	- The color of border and background.
 *
 *	- The location of the window.
 *	- The width, height of the window with borders. If window is not maximized, the width/height should always be width/height of drawing box plus two times border width/heiht.
 *	- The width, height of the drawing box of the window. It is calculated as cols/rows of terminal times font width/height.
 *	- The border width of this window (vertical and horizontal).
 *
 *	- Bit Flag: We should nuke 'window' when done with it
 *	- Bit Flag: This window is currently shown
 *	- Bit Flag: 1st extra flag
 *	- Bit Flag: 2nd extra flag
 *
 *	- Bit Flag: 3rd extra flag
 *	- Bit Flag: 4th extra flag
 *	- Bit Flag: 5th extra flag
 *	- Bit Flag: 6th extra flag
 */
struct infowin {
	SDL_Window		*window;
	SDL_Surface	*surface;

	uint32_t	windowID;

	Pixel		b_color, bg_color;

	int16_t		x, y;
	uint16_t		wb, hb;
	uint16_t		wd, hd;
	uint16_t		bw, bh;

	uint32_t		nuke:1;
	uint32_t		shown:1;
};

//TODO jezek - Continue refactoring & structure alignment.
/*
 * A Structure summarizing Operation+Color Information
 *
 *	- The Foreground Pixel Value
 *	- The Background Pixel Value
 *
 */
struct infoclr {
	Pixel		fg;
	Pixel		bg;

	//TODO jezek - Continue. Get rid of xor. It is used only for cursor. Use transparency for cursor.
	bool		xor;
};


/*
 * A Structure to Hold Font Information
 *
 *	- The font type
 *	- The 'TTF_Font*' or 'PCF_Font' (yields the 'Font')
 *
 *	- The font name
 *
 *	- The default character width
 *	- The default character height
 *
 *	- Flag: Force monospacing via 'wid'
 *	- Flag: Nuke font when done
 */
typedef enum {
    FONT_TYPE_TTF,
    FONT_TYPE_PCF
} FontType;

struct infofnt {
	// If type is `FONT_TYPE_TTF`, then font is `TTF_Font*`.
	// If type is `FONT_TYPE_PCF`, then font is `PCF_Font*`.
	FontType	type;
	void			*font;

	cptr			name;

	s16b			wid;
	s16b			hgt;

	uint			emulateMonospace:1;
	uint			nuke:1;
};

/**** Available Macros ****/

/* Pixel helper macros */
#define Pixel_equal(c1,c2) memcmp(&c1, &c2, sizeof(Pixel)) == 0
#define Pixel_quadruplet(color) color.r, color.g, color.b, color.a
#define Pixel_triplet(color) color.r, color.g, color.b

/* Set the current Infowin */
#define Infowin_set(I) \
	(Infowin = (I))


/* Set the current Infoclr */
#define Infoclr_set(C) \
	(Infoclr = (C))

#define Infoclr_init_parse(F, B, xor) \
	Infoclr_init_data(Infoclr_Pixel(F), Infoclr_Pixel(B), xor)


/* Set the current infofnt */
#define Infofnt_set(I) \
	(Infofnt = (I))


/* 
 * Default color values.
 * Colors can be redefined during init.
 */
Pixel color_default_bg = (Pixel){0x00, 0x00, 0x00, 0xFF};
Pixel color_default_fg = (Pixel){0xFF, 0xFF, 0xFF, 0xFF};
Pixel color_default_b  = (Pixel){0xFF, 0xFF, 0xFF, 0xFF};

/*
 * The "current" variables
 */
static infowin *Infowin = (infowin*)(NULL);
static infoclr *Infoclr = (infoclr*)(NULL);
static infofnt *Infofnt = (infofnt*)(NULL);


/*
 * Set the name (in the title bar) of Infowin
 */
static errr Infowin_set_name(cptr name) {
	char buf[128];
	char *bp = buf;

	strcpy(buf, name);
	SDL_SetWindowTitle(Infowin->window, bp);

	return(0);
}

/*
 * Init an infowin by giving some data.
 *
 * Inputs:
 *	x,y: The position of this Window
 *	w,h: The size of this Window
 *	b:   The border width
 *	b_color, bg_color: Border and background colors
 *
 */
static errr Infowin_init(int x, int y, int w, int h, unsigned int b, Pixel b_color, Pixel bg_color) {
	fprintf(stderr, "jezek - Infowin_init(int x: %d, int y: %d, int w: %d, int h: %d, unsigned int b: %d, Pixel b_color: %x, Pixel bg_color: %x)\n", x, y, w, h, b, *(uint32_t*)&b_color, *(uint32_t*)&bg_color);

	/* Wipe it clean */
	WIPE(Infowin, infowin);

	/*** Create the SDL_Window* 'window' from data ***/

	/* Create the Window. */
	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	if (!window_decorations) flags |= SDL_WINDOW_BORDERLESS;
	SDL_Window *window = SDL_CreateWindow("", x, y, w, h, flags);

	if (window == NULL) {
		fprintf(stderr, "Error creating window in Infowin_init\n");
		return(1);
	}

	SDL_Surface *surface = SDL_GetWindowSurface(window);
	SDL_FillRect(surface, &(SDL_Rect){0, 0, w, h}, SDL_MapRGBA(surface->format, Pixel_quadruplet(b_color)));
	SDL_FillRect(surface, &(SDL_Rect){b, b, w-(2*b), h-(2*b)}, SDL_MapRGBA(surface->format, Pixel_quadruplet(bg_color)));
	SDL_UpdateWindowSurface(window);

	/* Assign stuff */
	Infowin->windowID = SDL_GetWindowID(window);
	Infowin->window = window;
	Infowin->surface = surface;

	/* Apply the above info */
	Infowin->x = x;
	Infowin->y = y;
	Infowin->wb = w;
	Infowin->hb = h;
	Infowin->wd = w-(2*b);
	Infowin->hd = h-(2*b);
	Infowin->bw = b;
	Infowin->bh = b;
	Infowin->b_color = b_color;
	Infowin->bg_color = bg_color;

	uint32_t windowFlags = SDL_GetWindowFlags(window);
	Infowin->shown = windowFlags & SDL_WINDOW_SHOWN;

	// Mark it as nukeable.
	Infowin->nuke = 1;

	return(0);
}


/*
 * Request a Pixel by name.
 *
 * Inputs:
 *      name: The name of the color to try to load (see below)
 *
 * Output:
 *	The Pixel value that matched the given name
 *	'color_default_fg' if the name was unparseable
 *
 * Valid forms for 'name':
 *	'fg', 'bg', '<color-name>' and '#<hex-code>'
 */
static Pixel Infoclr_Pixel(cptr name) {

	/* Attempt to Parse the name */
	if (name && name[0]) {
		/* The 'bg' color is available */
		if (streq(name, "bg")) return color_default_bg;

		/* The 'fg' color is available */
		if (streq(name, "fg")) return color_default_fg;

		/* The 'white' color is available */
		if (streq(name, "white")) return (Pixel){0xFF, 0xFF, 0xFF, 0xFF};

		/* The 'black' color is available */
		if (streq(name, "black")) return (Pixel){0x00, 0x00, 0x00, 0xFF};

		/* 16 Basic Terminal Colors */
		if (streq(name, "red")) return (Pixel){0xAA, 0x00, 0x00, 0xFF};
		if (streq(name, "green")) return (Pixel){0x00, 0xAA, 0x00, 0xFF};
		if (streq(name, "yellow")) return (Pixel){0xAA, 0x55, 0x00, 0xFF};
		if (streq(name, "blue")) return (Pixel){0x00, 0x00, 0xAA, 0xFF};
		if (streq(name, "magenta")) return (Pixel){0xAA, 0x00, 0xAA, 0xFF};
		if (streq(name, "cyan")) return (Pixel){0x00, 0xAA, 0xAA, 0xFF};
		if (streq(name, "gray")) return (Pixel){0xAA, 0xAA, 0xAA, 0xFF};
		if (streq(name, "dark_red")) return (Pixel){0x55, 0x00, 0x00, 0xFF};
		if (streq(name, "dark_green")) return (Pixel){0x00, 0x55, 0x00, 0xFF};
		if (streq(name, "dark_yellow")) return (Pixel){0x55, 0x55, 0x00, 0xFF};
		if (streq(name, "dark_blue")) return (Pixel){0x00, 0x00, 0x55, 0xFF};
		if (streq(name, "dark_magenta")) return (Pixel){0x55, 0x00, 0x55, 0xFF};
		if (streq(name, "dark_cyan")) return (Pixel){0x00, 0x55, 0x55, 0xFF};
		if (streq(name, "dark_gray")) return (Pixel){0x55, 0x55, 0x55, 0xFF};
		if (streq(name, "light_gray")) return (Pixel){0xD3, 0xD3, 0xD3, 0xFF};

		/* If the format is '#......', parse the color */
		if (name[0] == '#' && strlen(name) == 7) {
			uint32_t color;
			if (sscanf(name + 1, "%06x", &color) == 1) {
				return 	(Pixel){
					.r = (color >> 16) & 0xFF,
					.g = (color >> 8) & 0xFF,
					.b = color & 0xFF,
					.a = 0xFF
				};
			}
		}

		plog_fmt("Warning: Couldn't parse color '%s'\n", name);
	}

	/* Warn about the Default being Used */
	plog_fmt("Warning: Using 'fg' for unknown color '%s'\n", name);

	/* Default to the 'Foreground' color */
	return color_default_fg;
}

/*
 * Initialize an infoclr with some data
 *
 * Inputs:
 *	fg:   The Pixel for the requested Foreground (see above)
 *	bg:   The Pixel for the requested Background (see above)
 */
static errr Infoclr_init_data(Pixel fg, Pixel bg, bool xor) {
	infoclr *iclr = Infoclr;

	/*** Initialize ***/

	/* Wipe the iclr clean */
	WIPE(iclr, infoclr);

	/* Assign the parms */
	iclr->fg = fg;
	iclr->bg = bg;
	iclr->xor = xor;

	/* Success */
	return(0);
}


static cptr ANGBAND_DIR_XTRA_FONT;

/*
 * Init an infofnt by its Name and Size. Assumes font is a true type font.
 *
 * Inputs:
 *	name: The name of the requested Font followed by a space and a number, which defines the size of font to load.
 */
static errr Infofnt_init_ttf(cptr name) {
	//fprintf(stderr, "jezek - Infofnt_init_ttf(cptr name: \"%s\"\n", name);
	/*** Load the info Fresh, using the name ***/

	/* If the name is not given, report an error */
	if (!name) return(-1);

	TTF_Font *font;
	char font_name[256];
	int font_size;

	/* Parse the name to extract font name and size */
	if (sscanf(name, "%255s %d", font_name, &font_size) != 2) {
		/* If the format is incorrect, return an error */
		fprintf(stderr, "Font \"%s\" does not match pattern \"<font_name> <font_size>\"", name);
		return(-1);
	}
	//fprintf(stderr, "jezek -  Infofnt_init_ttf: load font %s, size %d\n", font_name, font_size);

	char buf[1024];
	path_build(buf, 1024, ANGBAND_DIR_XTRA_FONT, font_name);

	//fprintf(stderr, "jezek -  Infofnt_init_ttf: load font path %s\n", buf);
	/* Attempt to load the font with the given size */
	font = TTF_OpenFont(buf, font_size);

	/* The load failed, try to recover */
	if (!font) {
		fprintf(stderr, "Loading font \"%s\" error: %s\n", buf, TTF_GetError());
		return(-1);
	}


	/*** Init the font ***/

	/* Wipe the thing */
	WIPE(Infofnt, infofnt);

	/* Assign the struct */
	Infofnt->type = FONT_TYPE_TTF;
	Infofnt->font = font;

	/* Extract default sizing info */
	Infofnt->hgt = TTF_FontHeight(font);
	/* Assume monospaced font */
	if (TTF_FontFaceIsFixedWidth(font) == 1) {
		Infofnt->emulateMonospace = false;
		/* Check all printable basic ASCII characters (32 to 126) for consistent width and height */
		char str[3] = "  ";
		SDL_Surface *surface;
		for (int ch = 32; ch <= 126; ++ch) {
			str[0] = (char)ch;
			surface = TTF_RenderText_Solid(font, str, (SDL_Color){255, 255, 255});
			if (surface == NULL) {
				fprintf(stderr, "Failed to render character %d!\n", ch);
				/* Free the font */
				TTF_CloseFont(font);
				/* Fail */
				return(-1);
			}

			int str_w = surface->w;
			SDL_FreeSurface(surface);

			if (ch == 32) {
				/* Assign width and height based on the first character */
				Infofnt->wid = str_w/2;
			} else {
				/* Verify that all characters have the same width and height */
				if (str_w != 2*Infofnt->wid) {
					fprintf(stderr, "Dimensions mismatch for string \"%s\". Width is %d, expected %d.\n", str, str_w, 2*Infofnt->wid);
					/* Fail */
					Infofnt->emulateMonospace=true;
					break;
				}
			}
		}
	} else {
		if (Infofnt->emulateMonospace != true) {
			fprintf(stderr, "Font reports it is not monospace, turning on monospace emulation.\n");
			Infofnt->emulateMonospace=true;
		}
	}
	//fprintf(stderr, "jezek - Infofnt_prepare: hgt: %d, wid: %d, emulateMonospace: %b\n", Infofnt->hgt, Infofnt->wid, Infofnt->emulateMonospace);

	/* Save a copy of the font name */
	Infofnt->name = string_make(name);

	/* Mark it as nukable */
	Infofnt->nuke = 1;

	/* Success */
	return(0);
}

typedef struct PCF_Font PCF_Font;

struct PCF_Font {
	uint32_t    nGlyphs;
	uint16_t    glyphWidth, glyphHeight;
	int16_t     *glyphIndexes;
	SDL_Surface *bitmap;
};

static void PCF_CloseFont(PCF_Font *font);
static PCF_Font* PCF_OpenFont(const char *name);
SDL_Surface* PCF_RenderText(PCF_Font* font, const char* str, SDL_Color bg_color, SDL_Color fg_color);

/*
 * Init an infofnt by its Name. Assumes font is a monospaced .pcf bitmap font.
 *
 * Inputs:
 *	name: The name of the requested Font without extension.
 */
static errr Infofnt_init_pcf(cptr name) {
	fprintf(stderr, "jezek - Infofnt_init_pcf(name: \"%s\"\n", name);
	/*** Load the info Fresh, using the name ***/

	/* If the name is not given, report an error */
	if (!name) return(-1);

	PCF_Font *font;
	char font_name[256];
	size_t len = strlen(name);
	
	/* Append .pcf extension if missing */
	if ((len >= 4) && strcasecmp(name + len - 4, ".pcf") == 0) {
		strncpy(font_name, name, sizeof(font_name));
	} else {
		/* Add .pcf extension. */
		snprintf(font_name, sizeof(font_name), "%s.pcf", name);
	}
	font_name[sizeof(font_name) - 1] = '\0';

	fprintf(stderr, "jezek -  Infofnt_init_pcf: load font %s\n", font_name);

	char buf[1024];
	path_build(buf, 1024, ANGBAND_DIR_XTRA_FONT, font_name);

	/* Attempt to load the font with the given size */
	font = PCF_OpenFont(buf);

	/* The load failed, try to recover */
	if (!font) {
		fprintf(stderr, "Loading font \"%s\" error\n", buf);
		return(-1);
	}

	fprintf(stderr, "jezek - PCF font %s loaded w: %d, h: %d\n", buf, font->glyphWidth, font->glyphHeight);

	/*** Init the font ***/

	/* Wipe the thing */
	WIPE(Infofnt, infofnt);

	/* Assign the struct */
	Infofnt->type = FONT_TYPE_PCF;
	Infofnt->font = font;

	/* Extract default sizing info */
	Infofnt->wid = font->glyphWidth;
	Infofnt->hgt = font->glyphHeight;
	Infofnt->emulateMonospace = false;

	/* Save a copy of the font name */
	Infofnt->name = string_make(name);

	/* Mark it as nukable */
	Infofnt->nuke = 1;

	/* Success */
	return(0);
}

/*
 * Detect if the filename is acceptable for a PCF font:
 *  - returns true for names without any extension
 *  - returns true for ".pcf"  (case-insensitive)
 *  - returns false for any other extension
 */
bool is_pcf_font(const char *name)
{
	if (!name) return false;

	/* Trim trailing whitespace. */
	size_t len = strlen(name);
	while (len > 0 && isspace((unsigned char)name[len - 1])) --len;

	/* Check for empty string after trimming. */
	if (len == 0) return false;

	/* Mark end of string. One past the last real char. */
	const char *end = name + len;

	/* Find the last '.' that is *after* the last path separator. */
	const char *dot = NULL;
	for (const char *p = end; p-- > name; ) {
		/* Reached a path separator.  */
		if (*p == '/' || *p == '\\') break;

		/* Found a potential extension. */
		if (*p == '.') {
			dot = p;
			break;
		}
	}

	/* No '.'  → no extension. */
	if (!dot) return true;

	/* Compare the extension including the dot. */
	if ((end - dot) == 4 && strncasecmp(dot, ".pcf", 4) == 0) return true;

	/* Other extension. */
	return false;
}

/*
 * Detect if `name` refers to a TTF font.
 * ────────────────────────────────────────────────────────────────
 * - If the filename ends with “.ttf” (case-insensitive), returns
 *   true; otherwise returns false.
 * - If `out_name` != NULL and `out_name_len` > 0, copies the
 *   *sanitized* basename (the part up to and including ".ttf",
 *   without trailing spaces or size suffix) into that buffer.
 * - If `out_size` != NULL, stores the parsed size; stores -1 when
 *   no numeric suffix is present.
 *
 * No dynamic allocation happens inside this function, so the caller
 * never has to `free()` anything.
 */
bool is_ttf_font(const char *name, char *out_name, size_t out_name_len, int8_t *out_size) {
	const char *ext;
	const char *p;
	int8_t size = -1;
	char tmp[256];
	size_t len;

	/* Initialise optional outputs. */
	if (out_name && out_name_len) *out_name = '\0';
	if (out_size) *out_size = -1;

	if (!name || !*name) return false;

	/* Copy and truncate to local buffer. */
	len = strlen(name);
	if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
	memcpy(tmp, name, len);
	tmp[len] = '\0';

	/* Trim trailing whitespace. */
	while (len && isspace((unsigned char)tmp[len - 1])) tmp[--len] = '\0';

	/* Locate extension. */
	ext = strrchr(tmp, '.');
	if (!ext || strncasecmp(ext, ".ttf", 4) != 0)
		return false;

	/* Parse optional numeric size after ".ttf". */
	/* Point just past ".ttf". */
	p = ext + 4;
	while (isspace((unsigned char)*p)) p++;
	if (*p) {
		/* There is something after the spaces. */
		char *endptr;
		long val = strtol(p, &endptr, 10);
		/* Not a number. */
		if (endptr == p) return false;
		while (isspace((unsigned char)*endptr)) endptr++;
		/* Junk after number. */
		if (*endptr) return false;
		size = (int8_t)val;
	}

	/* Copy sanitized basename into caller-supplied buffer. */
	if (out_name && out_name_len) {
		/* Include ".ttf". */
		size_t base_len = (ext - tmp) + 4;
		if (base_len >= out_name_len) base_len = out_name_len - 1;
		memcpy(out_name, tmp, base_len);
		out_name[base_len] = '\0';
	}

	if (out_size) *out_size = size;

	return true;
}

/*
 * Init an infofnt by its Name
 *
 * Inputs:
 *	name: The name of the requested Font
 */
static errr Infofnt_init(cptr name) {
	/* TrueType fonts are assumed unless the file name denotes a PCF font. */
	if (is_pcf_font(name)) {
		return Infofnt_init_pcf(name);
	}
	return Infofnt_init_ttf(name);

}

/*
 * Request that Infowin be raised
 */
static errr Infowin_raise(void) {
	/* Raise towards visibility */
	SDL_RaiseWindow(Infowin->window);

	/* Success */
	return(0);
}

/*
 * Request to focus Infowin.
 */
static errr Infowin_set_focus(void) {
	/* No input focus for window in SDL2. Try raise window. */
	SDL_RaiseWindow(Infowin->window);

	/* Success */
	return(0);
}

/*
 * Move an infowin.
 */
static errr Infowin_move(int x, int y) {
	/* Execute the request */
	Infowin->x = x;
	Infowin->y = y;
	SDL_SetWindowPosition(Infowin->window, x, y);

	/* Success */
	return(0);
}

/*
 * Resize an infowin
 */
static errr Infowin_resize(int w, int h) {
	fprintf(stderr, "jezek - Infowin_resize(w: %d, h: %d)\n", w, h);
	//TODO jezek - There are no window hints in sdl2, be sure to resize properly (w_inc. h_inc).
	/* Execute the request */
	Infowin->wb = w;
	Infowin->hb = h;
	SDL_SetWindowSize(Infowin->window, w, h);

	/* Success */
	return(0);
}

/*
 * Visually clear Infowin
 */
static errr Infowin_wipe(void) {
	// Draw borders.

	//TODO jezek - Allow define border color width & color in config.
	uint8_t r = Infowin->b_color.r;
	uint8_t g = Infowin->b_color.g;
	uint8_t b = Infowin->b_color.b;
	uint8_t a = Infowin->b_color.a;
	uint32_t bc = SDL_MapRGBA(Infowin->surface->format, r, g, b, a);
	// Draw top border.
	SDL_FillRect(Infowin->surface, &(SDL_Rect){0, 0, Infowin->wb, Infowin->bh}, bc);
	// Draw bottom border.
	SDL_FillRect(Infowin->surface, &(SDL_Rect){0, Infowin->bh + Infowin->hd, Infowin->wb, (Infowin->hb - Infowin->hd - Infowin->bh)}, bc);
	// Draw left border.
	SDL_FillRect(Infowin->surface, &(SDL_Rect){0, Infowin->bh + 1, Infowin->bw, Infowin->hd}, bc);
	// Draw right border.
	SDL_FillRect(Infowin->surface, &(SDL_Rect){Infowin->bw + Infowin->wd, Infowin->bh + 1, Infowin->wb - Infowin->wd - Infowin->bw, Infowin->hd}, bc);

	// Wipe drawing field.
	SDL_FillRect(Infowin->surface, &(SDL_Rect){Infowin->bw, Infowin->bh, Infowin->wd, Infowin->hd}, SDL_MapRGBA(Infowin->surface->format, Pixel_quadruplet(Infowin->bg_color)));

	/* Success */
	return(0);
}

/*
 * Standard Text
 */
static errr Infofnt_text_std(int x, int y, cptr str, int len) {
	//int ty = y;
	//if (ty==0) fprintf(stderr, "jezek - Infofnt_text_std(int x: %d, int y: %d, cptr str: \"%s\", int len: %d)\n", x, y, str, len);
	if (!str || !*str) return(-1);


	if (len < 0) len = strlen (str);
	//if (len < strlen(str)) str[len]='\0';

	if (len == 0) return(0);

	if (Infofnt->emulateMonospace && len > 1) {
		for (int i = 0; i < len; ++i) {
			if (Infofnt_text_std(x + i, y, str + i, 1)) {
				fprintf(stderr, "Failed to display the letter '%c' onto the position x, y = %d, %d.\n", str[i], x+i, y);
			}
		}
		return(0);
	}

	//if (ty==0) fprintf(stderr, "jezek - Infofnt_text_std: hgt: %d, wid: %d\n", Infofnt->hgt, Infofnt->wid);
	x = Infowin->bw + (x * Infofnt->wid);
	y = Infowin->bh + (y * Infofnt->hgt);

	/* Create surface and text texture */
	SDL_Color fgColor = {Pixel_quadruplet(Infoclr->fg)};
	SDL_Color bgColor = {Pixel_quadruplet(Infoclr->bg)};

	SDL_Surface *surface = NULL;
	if (Infofnt->type == FONT_TYPE_TTF) {
		surface = TTF_RenderText_LCD((TTF_Font*)Infofnt->font, str, fgColor, bgColor);
	} else if (Infofnt->type == FONT_TYPE_PCF) {
		surface = PCF_RenderText((PCF_Font*)Infofnt->font, str, fgColor, bgColor);
	}
	if (!surface) {
		fprintf(stderr, "Failed to create surface for text: %s\n", str);
		return(1);
	}


	//if (ty==0) fprintf(stderr, "jezek -  Infofnt_text_std: x: %d, y: %d, w: %d, h: %d, sw: %d, sh: %d\n", x, y, Infofnt->wid, Infofnt->hgt, surface->w, surface->h);
	SDL_Rect srcRect = {0, 0, Infofnt->wid*len, Infofnt->hgt};
	//SDL_Rect dstRect = {x, y, surface->w, surface->h};
	SDL_Rect dstRect = {x, y, Infofnt->wid*len, Infofnt->hgt};


	if (Infoclr->xor == true) {
		fprintf(stderr, "jezek - xor in font not implemented");
	} else {
		SDL_BlitSurface(surface, &srcRect, Infowin->surface, &dstRect);
	}

	SDL_FreeSurface(surface);

	/* Success */
	return(0);
}

//TODO jezek - Remove SDL_xor_rect function after removing xor painting.
void SDL_xor_rect(SDL_Surface *surface, SDL_Rect rect, Uint32 color) {
    // Lock the surface if required
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) < 0) {
        fprintf(stderr, "Failed to lock the surface: %s\n", SDL_GetError());
        return;
    }

    // Access pixel data
    Uint8 *pixels = (Uint8 *)surface->pixels;
    int pitch = surface->pitch; // Number of bytes in one row
    int bpp = surface->format->BytesPerPixel; // Bytes per pixel

    for (int row = rect.y; row < rect.y + rect.h; row++) {
        for (int col = rect.x; col < rect.x + rect.w; col++) {
            // Ensure the pixel is within the surface bounds
            if (row >= 0 && row < surface->h && col >= 0 && col < surface->w) {
                // Calculate the address of the pixel
                Uint8 *pixel = pixels + row * pitch + col * bpp;

                // Read the existing pixel value
                Uint32 pixel_value;
                switch (bpp) {
                    case 1:
                        pixel_value = *pixel;
                        break;
                    case 2:
                        pixel_value = *(Uint16 *)pixel;
                        break;
                    case 3: {
                        Uint8 r, g, b;
                        SDL_GetRGB(*(Uint32 *)pixel, surface->format, &r, &g, &b);
                        pixel_value = SDL_MapRGB(surface->format, r, g, b);
                        break;
                    }
                    case 4:
                        pixel_value = *(Uint32 *)pixel;
                        break;
                    default:
                        pixel_value = 0;
                        break;
                }

                // Apply XOR operation
                pixel_value ^= color;

                // Write the updated pixel value back
                switch (bpp) {
                    case 1:
                        *pixel = (Uint8)pixel_value;
                        break;
                    case 2:
                        *(Uint16 *)pixel = (Uint16)pixel_value;
                        break;
                    case 3: {
                        Uint8 r, g, b;
                        SDL_GetRGB(pixel_value, surface->format, &r, &g, &b);
                        *(pixel + 0) = r;
                        *(pixel + 1) = g;
                        *(pixel + 2) = b;
                        break;
                    }
                    case 4:
                        *(Uint32 *)pixel = pixel_value;
                        break;
                }
            }
        }
    }

    // Unlock the surface if it was locked
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}
/*
 * Painting where text would be
 */
static errr Infofnt_text_non(int x, int y, cptr str, int len) {
	//fprintf(stderr, "jezek - Infofnt_text_non(int x: %d, int y: %d, cptr str: \"%s\", int len: %d)\n", x, y, str, len);

	// Negative length is a flag to count the characters in str.
	if (len <= 0) len = strlen(str);

	int w = len * Infofnt->wid;
	int h = Infofnt->hgt;

	x = Infowin->bw + (x * Infofnt->wid);
	y = Infowin->bh + (y * h);


	/*** Find other dimensions ***/

	/* Simply do 'Infofnt->hgt' (a single row) high */


	/*** Actually 'paint' the area ***/
	//fprintf(stderr, "jezek -  Infofnt_text_non: x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

	if (Infoclr->xor) {
		SDL_xor_rect(Infowin->surface, (SDL_Rect){x, y, w, h}, SDL_MapRGBA(Infowin->surface->format, Pixel_quadruplet(Infoclr->fg)));
	} else {
		/* Just do a Fill Rectangle */
		SDL_FillRect(Infowin->surface, &(SDL_Rect){x, y, w, h}, SDL_MapRGBA(Infowin->surface->format, Pixel_quadruplet(Infoclr->fg)));
	}

	/* Success */
	return(0);
}

/*
 * Hack -- cursor color
 */
static infoclr *xor;

/*
 * Color table
 */
#ifndef EXTENDED_BG_COLOURS
 static infoclr *clr[CLIENT_PALETTE_SIZE];
#else
 static infoclr *clr[CLIENT_PALETTE_SIZE + TERMX_AMT];
#endif

/*
 * Forward declare
 */
typedef struct term_data term_data;
static int term_data_to_term_idx(term_data *td);
void terminal_window_real_coords_sdl2(int term_idx, int *ret_x, int *ret_y);
void resize_main_window_sdl2(int cols, int rows);
void resize_window_sdl2(int term_idx, int cols, int rows);
static term_data* term_idx_to_term_data(int term_idx);

//TODO jezek - Hide `disable_tile_cache` into TILE_CACHE_SIZE ifdef everywhere in code.
bool disable_tile_cache = FALSE;
#if defined(USE_GRAPHICS) && defined(TILE_CACHE_SIZE)

struct tile_cache_entry {
	SDL_Surface *tile;
	char32_t index;
	uint16_t ncolors;
	uint32_t *colors;
};
#endif

// A structure for each "term".
struct term_data {
	term t;

	infofnt *fnt;
	infowin *win;
	uint32_t resize_timer;

#ifdef USE_GRAPHICS

	// Number of layers in 'tiles_layers'. When using graphics and everything is properly initialized, this is always 'graphics_image_mpt - 1' (number of foreground mask colors).
	uint8_t nlayers;
	// Loaded tiles image, resized and split into multiple layers. Number of layers is 'nlayers'.
	// Each layer, except the first, contains only pixels, that belongs to it's mask color. All other pixels are fully transparent black color. The first layer contains also all other non-background mask color pixels.
	// Each layer has blend mode set to SDL_BLENDMODE_NONE and color key set to it's foreground mask color. This way, if you blit/blend the layer tile over a colored surface, you'll get a layer surface with desired color and transparent background. These layers blended over a tile colored with background color will create full tile with colors changed to desired values.
	SDL_Surface **tiles_layers;
	// Used for preparing a tile layer during tile rendering. The size is always same as 'fnt' font size.
	SDL_Surface *tilePreparation;

 #ifdef TILE_CACHE_SIZE
	int cache_position;
	struct tile_cache_entry tile_cache[TILE_CACHE_SIZE];
 #endif

#endif
};

#define DEFAULT_SDL2_BORDER_WIDTH 1

// The main screen.
static term_data term_main;
// The optional windows
static term_data term_1;
static term_data term_2;
static term_data term_3;

/*
 * Other (optional) windows.
 */
static term_data term_4;
static term_data term_5;
static term_data term_6;
static term_data term_7;
static term_data term_8;
static term_data term_9;

static term_data *sdl2_terms_term_data[ANGBAND_TERM_MAX] = {&term_main, &term_1, &term_2, &term_3, &term_4, &term_5, &term_6, &term_7, &term_8, &term_9};
static char *sdl2_terms_font_env[ANGBAND_TERM_MAX] = {"TOMENET_SDL2_FONT_TERM_MAIN", "TOMENET_SDL2_FONT_TERM_1", "TOMENET_SDL2_FONT_TERM_2", "TOMENET_SDL2_FONT_TERM_3", "TOMENET_SDL2_FONT_TERM_4", "TOMENET_SDL2_FONT_TERM_5", "TOMENET_SDL2_FONT_TERM_6", "TOMENET_SDL2_FONT_TERM_7", "TOMENET_SDL2_FONT_TERM_8", "TOMENET_SDL2_FONT_TERM_9"};
static char *sdl2_terms_font_default[ANGBAND_TERM_MAX] = {DEFAULT_SDL2_FONT_TERM_MAIN, DEFAULT_SDL2_FONT_TERM_1, DEFAULT_SDL2_FONT_TERM_2, DEFAULT_SDL2_FONT_TERM_3, DEFAULT_SDL2_FONT_TERM_4, DEFAULT_SDL2_FONT_TERM_5, DEFAULT_SDL2_FONT_TERM_6, DEFAULT_SDL2_FONT_TERM_7, DEFAULT_SDL2_FONT_TERM_8, DEFAULT_SDL2_FONT_TERM_9};
static int sdl2_terms_ttf_size_default[ANGBAND_TERM_MAX] = {DEFAULT_SDL2_TTF_FONT_SIZE_TERM_MAIN, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_1, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_2, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_3, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_4, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_5, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_6, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_7, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_8, DEFAULT_SDL2_TTF_FONT_SIZE_TERM_9};
const char *sdl2_terms_wid_env[ANGBAND_TERM_MAX] = {"TOMENET_SDL2_WID_TERM_MAIN", "TOMENET_SDL2_WID_TERM_1", "TOMENET_SDL2_WID_TERM_2", "TOMENET_SDL2_WID_TERM_3", "TOMENET_SDL2_WID_TERM_4", "TOMENET_SDL2_WID_TERM_5", "TOMENET_SDL2_WID_TERM_6", "TOMENET_SDL2_WID_TERM_7", "TOMENET_SDL2_WID_TERM_8", "TOMENET_SDL2_WID_TERM_9"};
const char *sdl2_terms_hgt_env[ANGBAND_TERM_MAX] = {"TOMENET_SDL2_HGT_TERM_MAIN", "TOMENET_SDL2_HGT_TERM_1", "TOMENET_SDL2_HGT_TERM_2", "TOMENET_SDL2_HGT_TERM_3", "TOMENET_SDL2_HGT_TERM_4", "TOMENET_SDL2_HGT_TERM_5", "TOMENET_SDL2_HGT_TERM_6", "TOMENET_SDL2_HGT_TERM_7", "TOMENET_SDL2_HGT_TERM_8", "TOMENET_SDL2_HGT_TERM_9"};

/*
 * Keycode macros, used on Keycode to test for classes of symbols.
 */
#define IsModifierKey(keycode) \
    ((keycode == SDLK_LSHIFT) || (keycode == SDLK_RSHIFT) || \
     (keycode == SDLK_LCTRL)  || (keycode == SDLK_RCTRL)  || \
     (keycode == SDLK_LALT)   || (keycode == SDLK_RALT)   || \
     (keycode == SDLK_LGUI)   || (keycode == SDLK_RGUI)   || \
     (keycode == SDLK_NUMLOCKCLEAR) || (keycode == SDLK_CAPSLOCK) || \
     (keycode == SDLK_MODE))

/*
 * Checks if the keysym is a special key or a normal key.
 * Special keys in SDL2 start from SDLK_WORLD_0 keycode.
 */
#define IsSpecialKey(keycode) \
    (((keycode) >= SDLK_F1 && (keycode) <= SDLK_F12) || \
     (keycode) == SDLK_ESCAPE || (keycode) == SDLK_RETURN || \
     (keycode) == SDLK_TAB || (keycode) == SDLK_BACKSPACE || \
     (keycode) == SDLK_DELETE || (keycode) == SDLK_HOME || \
     (keycode) == SDLK_END || (keycode) == SDLK_PAGEUP || \
     (keycode) == SDLK_PAGEDOWN || (keycode) == SDLK_INSERT || \
     (keycode) == SDLK_PRINTSCREEN || (keycode) == SDLK_PAUSE)

#ifdef SDL2_STICKY_KEYS
bool ctrlForced = false;
bool shiftForced = false;
#endif
static int Term_win_update(int flush, int sync, int discard);

/**
 * Function `shiftKeycode` transforms the given keycode to its shifted equivalent
 * based on a US ISO keyboard layout. If the character is already shifted or does not
 * have a shifted variant, the function returns the original character.
 *
 * @param keycode Input character (keycode)
 * @return Shifted character or the original character if no shifted variant exists
 */
char shiftKeycode(char keycode) {
    // Check if the character is a lowercase letter and convert it to uppercase
    if (keycode >= 'a' && keycode <= 'z') {
        return keycode - 'a' + 'A';
    }

    // Map special characters to their shifted equivalents
    switch (keycode) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        case '`': return '~';
        default:
            // If the character does not have a shifted variant or is already shifted, return it unchanged
            return keycode;
    }
}

/*
 * Process a keypress event
 */
static void react_keypress(SDL_Event *event) {
	SDL_Scancode scancode = event->key.keysym.scancode;
	SDL_Keycode keycode = event->key.keysym.sym;
	fprintf(stderr, "jezek - react_keypress: scancode: %d, keycode: %d\n", scancode, keycode);
	fprintf(stderr, "jezek -  react_keypress: tdidx: %d\n", term_data_to_term_idx((term_data*)(Term->data)));


	if (scancode == SDL_SCANCODE_UNKNOWN) {
		fprintf(stderr, "SDL2 react_keypress got unknown scancode in event.\n");
		return;
	}

	/* Ignore "modifier keys" */
	if (IsModifierKey(keycode)) {
		fprintf(stderr, "jezek -  react_keypress: modifier key\n");
#ifdef SDL2_STICKY_KEYS
		if (keycode == SDLK_LCTRL || keycode == SDLK_RCTRL) { ctrlForced = true; }
		if (keycode == SDLK_LSHIFT || keycode == SDLK_RSHIFT) { shiftForced = true; }
#endif
		return;
	}


	SDL_Keymod mod = SDL_GetModState();

	// Extract four "modifier flags"
	bool mc = (mod & KMOD_CTRL) ? true : false;
	bool ms = (mod & KMOD_SHIFT) ? true : false;
	bool mo = (mod & KMOD_ALT) ? true : false;
	bool mx = false; //(mod & KMOD_NUM) ? true : false;

	//fprintf(stderr, "jezek -  react_keypress: mc: %b, ms: %b, mo: %b, mx: %b\n", mc, ms, mo, mx);

#ifdef SDL2_STICKY_KEYS
	if (ctrlForced) {
		mc = true;
		ctrlForced = false;
	}

	if (shiftForced) {
		ms = true;
		shiftForced = false;
	}
#endif

	char msg[128] = "";

	if (keycode == SDLK_UNKNOWN) {
		// There is a unknown keycode, but scancode is known. Send special key sequence.
		fprintf(stderr, "jezek -  react_keypress: unknown keycode\n");
		sprintf(msg, "%c%s%s%s%sK_%X%c", 31,
				mc ? "N" : "", ms ? "S" : "",
				mo ? "O" : "", mx ? "M" : "",
				scancode, 13);

		for (int i = 0; msg[i]; i++) {
			fprintf(stderr,"jezek -  unknown Term_keypress(msg[i]: %d = '%c')\n", msg[i], msg[i]);
			Term_keypress(msg[i]);
		}
		return;
	}

	if (ms) {
		// Shift is pressed with the key, shift the keycode.
		keycode = shiftKeycode(keycode);
	}

	if (!mc && !mo && keycode < 256) {
		// No ctrl nor alt modifier keys are pressed, send the pressed keycode.
		// Keycode is already shifted, if shift is pressed.
		fprintf(stderr,"jezek -  plain Term_keypress(%d = '%c')\n", keycode, (char)(keycode));
		Term_keypress((char)(keycode));
		return;
	}

	if (mc && !mo && ((keycode >= 'a' && keycode <= 'z') || (keycode >= 'A' && keycode <= 'Z'))) {
		char control_char = keycode & 0x1F;  // Calculate the control character.
		fprintf(stderr, "jezek -  react_keypress: ctrl+%c -> control char: %d\n", keycode, control_char);
		Term_keypress(control_char);
		return;
	}

	int ks = -1;
	//fprintf(stderr, "jezek -  ks: %d\n", ks);

	/* Handle a few standard keys */
	switch (keycode) {
		case SDLK_ESCAPE:
			fprintf(stderr, "jezek -  react_keypress: escape key\n");
			Term_keypress(ESCAPE);
			return;

		case SDLK_RETURN:
			fprintf(stderr, "jezek -  react_keypress: enter key\n");
			Term_keypress('\r');
			return;

		case SDLK_TAB:
			fprintf(stderr, "jezek -  react_keypress: tab key\n");
			Term_keypress('\t');
			return;

		case SDLK_DELETE:
			fprintf(stderr, "jezek -  react_keypress: del key\n");
			//TODO jezek - Differentiate.
			Term_keypress('\010');
			return;

		case SDLK_BACKSPACE:
			fprintf(stderr, "jezek -  react_keypress: backspace key\n");
			//TODO jezek - Differentiate between delete and backspace key.
			Term_keypress('\010');
			return;

		case SDLK_UP:
			ks = 65362;
			break;

		case SDLK_RIGHT:
			ks = 65363;
			break;

		case SDLK_DOWN:
			ks = 65364;
			break;

		case SDLK_LEFT:
			ks = 65361;
			break;

	}

	//TODO jezek - Shift + arrows don't work? Test.
	if (ks > 0) {
		fprintf(stderr, "jezek -  ks: %d\n", ks);
		sprintf(msg, "%c%s%s%s%s_%lX%c", 31,
		        mc ? "N" : "", ms ? "S" : "",
		        mo ? "O" : "", mx ? "M" : "",
		        (unsigned long)(ks), 13);
	} else {
		sprintf(msg, "%c%s%s%s%s_%lX%c", 31,
		        mc ? "N" : "", ms ? "S" : "",
		        mo ? "O" : "", mx ? "M" : "",
		        (unsigned long)(keycode), 13);
	}

	// Enqueue the special key sequence.
	for (int i = 0; msg[i]; i++) {
		fprintf(stderr,"jezek -  last Term_keypress(msg[i]: %d = '%c')\n", msg[i], msg[i]);
		Term_keypress(msg[i]);
	}

}

/*
 * Handles all terminal windows resize timers.
 */
static void resize_window_sdl2_timers_handle(void) {
	uint32_t now = 0;
	int t_idx;

	for (t_idx = 0; t_idx < ANGBAND_TERM_MAX; t_idx++) {
		term_data *td = term_idx_to_term_data(t_idx);

		/* If resize_timer is nonzero, it represents the tick count after which we should resize. */
		if (td->resize_timer != 0) {
			/* Get the current SDL ticks once per loop. */
			if (now == 0) now = SDL_GetTicks();

			/* Check if the time has passed to resize. */
			if (now >= td->resize_timer) {
				td->resize_timer = 0;

				/* Calculate new columns/rows based on window border and font dimensions. */
				resize_window_sdl2(t_idx, (td->win->wb - (2 * td->win->bw)) / td->fnt->wid, (td->win->hb - (2 * td->win->bh)) / td->fnt->hgt);

				/* In case we resized Chat/Msg/Msg+Chat window,
				   refresh contents so they are displayed properly,
				   without having to wait for another incoming message to do it for us. */
				p_ptr->window |= PW_MESSAGE | PW_CHAT | PW_MSGNOCHAT;
				fprintf(stderr, "jezek - resize_window_sdl2_timers_handle PW_MESSAGE\n");
			}
		}
	}
}

/*
 * Process events
 */
static int CheckEvent(bool wait) {
	term_data *old_td = (term_data*)(Term->data);

	term_data *td = NULL;

	/* Handle all window resize timers. */
	resize_window_sdl2_timers_handle();

	SDL_Event event;

	if (!wait) {
		if (!SDL_PollEvent(&event)) return (1);
	} else {
		SDL_WaitEvent(&event);
	}

	/* Determine which window was the event for. */
	for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
		if (sdl2_terms_term_data[i]->win && sdl2_terms_term_data[i]->win->window && sdl2_terms_term_data[i]->win->windowID == event.window.windowID) {
			td = sdl2_terms_term_data[i];
			break;
		}
	}

	/* Unknown window */
	if (!td || !td->win) return(0);

	/* Hack -- activate the Term */
	Term_activate(&td->t);

	/* Hack -- activate the window */
	Infowin_set(td->win);

	switch (event.type) {
		case SDL_QUIT:
			fprintf(stderr, "jezek - Got SDL_QUIT event\n");
			return 1; // Signal to exit

		case SDL_KEYDOWN:
			fprintf(stderr, "jezek -  CheckEvent: SDL_KEYDOWN\n");

			/* Hack -- use "old" term */
			Term_activate(&old_td->t);

			/* Process the key */
			react_keypress(&event);

			break;

		case SDL_WINDOWEVENT:
				//fprintf(stderr, "jezek -  CheckEvent: SDL_WINDOWEVENT\n");
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				fprintf(stderr, "jezek -   CheckEvent: SDL_WINDOWEVENT_RESIZED\n");
				/* Handle window resize. */
				int new_wdt = event.window.data1;
				int new_hgt = event.window.data2;
				fprintf(stderr, "jezek -   CheckEvent: SDL_WINDOWEVENT_RESIZED: nw: %d, nh: %d, w: %d, h: %d\n", new_wdt, new_hgt, Infowin->wb, Infowin->hb);

				/* Check if window is resized. */
				bool resized = Infowin->wb != new_wdt || Infowin->hb != new_hgt;

				Infowin->wb = new_wdt;
				Infowin->hb = new_hgt;

				if (resized) {
					/* Window resize timer start. */
					td->resize_timer = SDL_GetTicks() + 500; // Add 1/2 second (500 ms).
				}
				break;
			}

			if (event.window.event == SDL_WINDOWEVENT_MOVED) {
				fprintf(stderr, "jezek -   CheckEvent: SDL_WINDOWEVENT_MOVED\n");
				Infowin->x = event.window.data1;
				Infowin->y = event.window.data2;

				int top, left, bottom, right;
				SDL_GetWindowBordersSize(Infowin->window, &top, &left, &bottom, &right);
				//fprintf(stderr, "jezek - borders - Top: %d, Left: %d, Bottom: %d, Right: %d\n", top, left, bottom, right);

				Infowin->x -= left;
				Infowin->y -= top;
				(void)bottom; (void)right;
				fprintf(stderr, "jezek -   CheckEvent: SDL_WINDOWEVENT_MOVED: x: %d, y: %d\n", Infowin->x, Infowin->y);
				break;
			}

		//default:
			//fprintf(stderr, "jezek - Got unknown event type: %d\n", event.type);

	}

	/* Hack -- Activate the old term */
	Term_activate(&old_td->t);

	/* Hack -- Activate the proper "inner" window */
	Infowin_set(old_td->win);

	/* Success */
	return(0);
}

#ifdef USE_GRAPHICS

/* Frees all graphics structures in provided term_data and sets them to zero values. */
static void free_graphics(term_data *td) {
	int i;

	if (td->nlayers > 0) {
		for (i = 0; i < td->nlayers; i++) {
			if (td->tiles_layers[i] != NULL) SDL_FreeSurface(td->tiles_layers[i]);
		}
		td->nlayers = 0;
		td->tiles_layers = NULL;
	}
	if (td->tilePreparation) {
		SDL_FreeSurface(td->tilePreparation);
		td->tilePreparation = NULL;
	}

 #ifdef TILE_CACHE_SIZE
	for (int i = 0; i < TILE_CACHE_SIZE; i++) {
		if (td->tile_cache[i].tile) {
			SDL_FreeSurface(td->tile_cache[i].tile);
			td->tile_cache[i].tile = NULL;
		}
		td->tile_cache[i].index = 0;
		if (td->tile_cache[i].ncolors > 0) {
			C_FREE(td->tile_cache[i].colors, td->tile_cache[i].ncolors, uint32_t);
			td->tile_cache[i].ncolors = 0;
		}
	}
 #endif
}
#endif

/* Closes all SDL2 windows and frees all allocated data structures for input parameter. */
static errr term_data_nuke(term_data *td) {
	//fprintf(stderr, "jezek - term_data_nuke(term_data *td: %p)\n", td);
	if (td == NULL) return(0);

#ifdef USE_GRAPHICS
	// Free graphics structures.
	if (use_graphics) {
		/* Free graphic tiles & masks. */
		free_graphics(td);
	}
#endif

	// Unmap & free inner window.
	if (td->win && td->win->nuke) {
		if (Infowin == td->win) Infowin_set(NULL);
		if (td->win->window) {
			SDL_FreeSurface(td->win->surface);
			SDL_DestroyWindow(td->win->window);
			td->win->windowID = -1;
		}
		FREE(td->win, infowin);
	}

	/* Reset timers just to be sure. */
	td->resize_timer = 0;

	// Free font.
	if (td->fnt && td->fnt->nuke) {
		if (Infofnt == td->fnt) Infofnt_set(NULL);
		if (td->fnt->font) {
			if (td->fnt->type == FONT_TYPE_TTF) TTF_CloseFont((TTF_Font*)td->fnt->font);
			if (td->fnt->type == FONT_TYPE_PCF) PCF_CloseFont((PCF_Font*)td->fnt->font);
		}
		if (td->fnt->name) string_free(td->fnt->name);
		FREE(td->fnt, infofnt);
	}

	return(0);
}

/* Saves terminal window position, dimensions and font for term_idx to term_prefs.
 * Note: The term_prefs visibility is not handled here. */
static void term_data_to_term_prefs(int term_idx) {
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;
	term_data *td = term_idx_to_term_data(term_idx);

	/* Update position. */
	term_prefs[term_idx].x = td->win->x;
	term_prefs[term_idx].y = td->win->y;

	//TODO jezek - Use td->t->wid/hgt?
	term_prefs[term_idx].columns = td->win->wd / td->fnt->wid;
	term_prefs[term_idx].lines = td->win->hd / td->fnt->hgt;
	fprintf(stderr, "jezek - term_data_to_term_prefs: tp.c: %d, tp.l: %d, td->t.wid: %d, td->t.hgt: %d\n", term_prefs[term_idx].columns, term_prefs[term_idx].lines, td->t.wid, td->t.hgt);

	/* Update font. */
	if (strcmp(term_prefs[term_idx].font, td->fnt->name) != 0) {
		strncpy(term_prefs[term_idx].font, td->fnt->name, sizeof(term_prefs[term_idx].font));
		term_prefs[term_idx].font[sizeof(term_prefs[term_idx].font) - 1] = '\0';
	}
}

/* For saving all window layout while client is still running (from within = menu) */
void all_term_data_to_term_prefs(void) {
	int n;

	for (n = 0; n < ANGBAND_TERM_MAX; n++) {
		if (!term_get_visibility(n)) continue;
		term_data_to_term_prefs(n);
	}
}

/*
 * Handle destruction of a term.
 * Here we should properly destroy all windows and resources for terminal.
 * But after this the whole client ends (should not recover), so just use it for filling terminal preferences, which will be saved after all terminals are nuked.
 */
static void Term_nuke_sdl2(term *t) {
	fprintf(stderr, "jezek - Term_nuke_sdl2(tidx: %d)\n", term_data_to_term_idx((term_data*)(t->data)));
	term_data *td = (term_data*)(t->data);
	int term_idx;

	/* special hack: this window was invisible, but we just toggled it to become visible on next client start. */
	if (!td->fnt) return;

	term_idx = term_data_to_term_idx(td);
	if (term_idx < 0) {
		fprintf(stderr, "Error getting terminal index from term_data\n");
		return;
	}

	term_data_to_term_prefs(term_idx);

	term_data_nuke(td);
	t->data = NULL;
}

/*
 * Handle "activation" of a term
 */
static errr Term_xtra_sdl2_level(int v) {
	term_data *td = (term_data*)(Term->data);

	/* Handle "activate" */
	if (v) {
		/* Activate the "inner" window */
		Infowin_set(td->win);

		/* Activate the "inner" font */
		Infofnt_set(td->fnt);
	}

	/* Success */
	return(0);
}

/*
 * General Flush/ Sync/ Discard routine
 */
static int Term_win_update(int flush, int sync, int discard) {
	/* Flush if desired */
	if (flush) {

		//struct timeval start_time;
		//struct timeval end_time;
		//gettimeofday(&start_time, NULL);

		/* Render prepared texture into the window renderer respecting borders. */
		term_data *td = (term_data*)(Term->data);

#ifdef SDL2_STICKY_KEYS
		if (td == &term_main) {
			uint8_t r = td->win->b_color.r;
			uint8_t g = td->win->b_color.g;
			uint8_t b = td->win->b_color.b;
			uint8_t a = td->win->b_color.a;
			if (ctrlForced) r ^= 0xFF;
			if (shiftForced) g ^= 0xFF;
			uint32_t bc = SDL_MapRGBA(td->win->surface->format, r, g, b, a);
			// Draw top, bottom, left and right border.
			SDL_FillRect(td->win->surface, &(SDL_Rect){0, 0, td->win->wb, td->win->bh}, bc);
			SDL_FillRect(td->win->surface, &(SDL_Rect){0, td->win->bh + td->win->hd, td->win->wb, (td->win->hb - td->win->hd - td->win->bh)}, bc);
			SDL_FillRect(td->win->surface, &(SDL_Rect){0, td->win->bh + 1, td->win->bw, td->win->hd}, bc);
			SDL_FillRect(td->win->surface, &(SDL_Rect){td->win->bw + td->win->wd, td->win->bh + 1, td->win->wb - td->win->wd - td->win->bw, td->win->hd}, bc);
			//fprintf(stderr, "jezek - Term_win_update: termid: %d, wb: %d, hb: %d, wd: %d, hd: %d, bw: %d, bh: %d\n", term_data_to_term_idx(td), td->win->wb, td->win->hb, td->win->wd, td->win->hd, td->win->bw, td->win->bh);
		}
#endif

		SDL_UpdateWindowSurface(td->win->window);
	}

	/* Sync if desired, using 'discard' */
	if (sync) {
		/* Call SDL_PumpEvents to update the input state, which can be analogous to sync operations */
		SDL_PumpEvents();
		//fprintf(stderr, "jezek - SDL_PumpEvents()\n");

		if (discard) {
			/* If we want to discard events, we can clear the event queue */
			SDL_FlushEvent(SDL_FIRSTEVENT);
			//fprintf(stderr, "jezek - SDL_FlushEvent(SDL_FIRSTEVENT)");
		}
	}

	/* Success */
	return 0;
}

/*
 * Handle a "special request"
 */
static errr Term_xtra_sdl2(int n, int v) {
	//fprintf(stderr, "jezek - Term_xtra_sdl2(tidx: %d, n: %d, v: %d)\n", term_data_to_term_idx((term_data*)(Term->data)), n, v);
	/* Handle a subset of the legal requests */
	switch (n) {
		/* Make a noise */
		case TERM_XTRA_NOISE: 
			//TODO jezek - Make a beep sound.
			return(0);

		/* Flush the output XXX XXX XXX */
		case TERM_XTRA_FRESH: Term_win_update(1, 0, 0); return(0);

		/* Process random events XXX XXX XXX */
		case TERM_XTRA_BORED: return(CheckEvent(0));

		/* Process Events XXX XXX XXX */
		case TERM_XTRA_EVENT: return(CheckEvent(v));

		/* Flush the events XXX XXX XXX */
		case TERM_XTRA_FLUSH: while (!CheckEvent(FALSE)); return(0);

		/* Handle change in the "level" */
		case TERM_XTRA_LEVEL: return(Term_xtra_sdl2_level(v));

		/* Clear the screen */
		case TERM_XTRA_CLEAR: Infowin_wipe(); return(0);

		/* Delay for some milliseconds */
		case TERM_XTRA_DELAY: SDL_Delay(v); return(0);
	}

	/* Unknown */
	return(1);
}

/*
 * Erase a number of characters
 */
static errr Term_wipe_sdl2(int x, int y, int n) {
	//fprintf(stderr, "jezek - Term_wipe_sdl2(tidx: %d, x: %d, y: %d, n: %d)\n", term_data_to_term_idx((term_data*)(Term->data)), x, y, n);
	/* Erase (use black) */
	Infoclr_set(clr[0]);

	/* Mega-Hack -- Erase some space */
	Infofnt_text_non(x, y, "", n);

	/* Success */
	return(0);
}

static int cursor_x = -1, cursor_y = -1;
/*
 * Draw the cursor (XXX by hiliting)
 */
static errr Term_curs_sdl2(int x, int y) {
	/*
	 * Don't place the cursor in the same place multiple times to avoid
	 * blinking.
	 */
	if ((cursor_x != x) || (cursor_y != y)) {
		/* Draw the cursor */
		Infoclr_set(xor);

		/* Hilite the cursor character */
		Infofnt_text_non(x, y, " ", 1);

		cursor_x = x;
		cursor_y = y;
	}

	/* Success */
	return(0);
}

/*
 * Draw a number of characters (XXX Consider using "cpy" mode)
 */
static errr Term_text_sdl2(int x, int y, int n, byte a, cptr s) {
#if 1 /* For 2mask mode: Actually imprint screen buffer with "empty background" for this text printed grid, to possibly avoid glitches */
 #ifdef USE_GRAPHICS
  #ifdef GRAPHICS_BG_MASK
	{
		byte *scr_aa_back = Term->scr_back->a[y];
		char32_t *scr_cc_back = Term->scr_back->c[y];

		byte *old_aa_back = Term->old_back->a[y];
		char32_t *old_cc_back = Term->old_back->c[y];

		old_aa_back[x] = scr_aa_back[x] = TERM_DARK;
   #if 0
		old_cc_back[x] = scr_cc_back[x] = 32;
   #else
		old_cc_back[x] = scr_cc_back[x] = Client_setup.f_char[FEAT_SOLID];
   #endif
	}
  #endif
 #endif
#endif
	/* Catch use in chat instead of as feat attr, or we crash :-s
	   (term-idx 0 is the main window; screen-pad-left check: In case it is used in the status bar for some reason; screen-pad-top checks: main screen top chat line or status line) */
	if (Term && Term->data == &term_main&& x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + SCREEN_WID && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + SCREEN_HGT) {
		flick_global_x = x;
		flick_global_y = y;
	} else flick_global_x = 0;

	a = term2attr(a);

	/* Draw the text in Xor */
#ifndef EXTENDED_COLOURS_PALANIM
 #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x0F]);
 #else
	Infoclr_set(clr[a & 0x1F]); //undefined case actually, we don't want to have a hole in the colour array (0..15 and then 32..32+x) -_-
 #endif
#else
 #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x1F]);
 #else
	Infoclr_set(clr[a & 0x3F]);
 #endif
#endif

	/* Draw the text */
	Infofnt_text_std(x, y, s, n);

	/* Drawing text seems to clear the cursor */
	if (cursor_y == y && x <= cursor_x && cursor_x <= x + n) {
		/* Cursor is gone */
		cursor_x = -1;
		cursor_y = -1;
	}

	/* Success */
	return(0);
}

#ifdef USE_GRAPHICS // Huge block.

// Directory with graphics tiles files (should be lib/xtra/grapics).
static cptr ANGBAND_DIR_XTRA_GRAPHICS;


// Loaded tiles image.
SDL_Surface *graphics_image = NULL;

/* These variables are computed at image load (in 'init_sdl2'). */
uint16_t graphics_tile_wid, graphics_tile_hgt;
// Tiles per row.
int16_t graphics_image_tpr;
// Masks per tile.
uint8_t graphics_image_mpt;
// Tiles per coordinate.
uint8_t graphics_image_tpc;
// Array of mask colors in RGBA format.
uint32_t graphics_image_masks_colors[GRAPHICS_MAX_MPT];
// Flag indicating that graphic tiles should be re/initialized for every window afrer 'sdl2_graphics_pref_file_processed' callback is called..
bool graphics_reinitialize;

static void term_data_init_graphics(term_data *td);
static errr Term_pict_sdl2_2mask(int x, int y, byte a, char32_t c, byte a_back, char32_t c_back);
static errr Term_pict_sdl2(int x, int y, byte a, char32_t c);
static uint32_t graphics_default_mask(uint8_t n);

// Gets called after graphics image pref file is loaded.
void sdl2_graphics_pref_file_processed() {
	fprintf(stderr, "jezek - Loaded graphics pref file\n");
	if (use_graphics && graphics_reinitialize) {

		// Check if masks colors are filled. Use default values if not.
		for (uint8_t i = 0; i < graphics_image_mpt; ++i) {
			if (graphics_image_masks_colors[i] == 0) {
				graphics_image_masks_colors[i] = (uint32_t)(graphics_default_mask(i));
				fprintf(stderr, "Warning: Mask color for mask no %d is not set in pref file. Using default value #%06x.\n", i, graphics_image_masks_colors[i]);
			}
		}
		//TODO jezek - If UG_2MASK and (only 2 mask colors or empty outline mask or forced outline creatio), create outline mask from background.

		// Initialize graphics for each initialized term.
		for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
			if (!term_get_visibility(i)) continue;

			term_data *td = term_idx_to_term_data(i);
			fprintf(stderr, "jezek - Reinitializing graphics for term id: %d\n", i);
			term_data_init_graphics(td);
			if (td->tiles_layers != NULL && td->tilePreparation != NULL) {
				/* Graphics hook */
#ifdef GRAPHICS_BG_MASK
				if (use_graphics == UG_2MASK) {
					td->t.pict_hook_2mask = Term_pict_sdl2_2mask;
				}
#endif
				td->t.pict_hook = Term_pict_sdl2;

				/* Use graphics sometimes */
				td->t.higher_pict = TRUE;
			}
			else {
				fprintf(stderr, "Couldn't prepare images for terminal %d\n", i);
			}
		}

		graphics_reinitialize = false;
	}
	fprintf(stderr, "jezek - reinitialization complete\n");
}

 #ifdef TILE_CACHE_SIZE
// Searches for a tile with provided index and colors. Returns a surface for that tile if found.
// When requested tile is not found, returns NULL.
// Assumes that the colors array size is `graphics_image_mpt`.
// The first color in the color array in input is ignored, because it is the background color, which is not stored in cache entry.
static SDL_Surface* graphics_tile_cache_search(term_data *td, char32_t index, uint32_t colors[]) {
	struct tile_cache_entry *entry;
	int i;

	for (i = 0; i < TILE_CACHE_SIZE; i++) {
		entry = &td->tile_cache[i];
		if (entry->index == index && memcmp(entry->colors, &colors[1], sizeof(uint32_t) * entry->ncolors) == 0) {
			return(entry->tile);
		}
	}
	return(NULL);
}

// Stores the index and colors in a new entry and returns a blank surface in which the tile we want to cache can be drawn.
// Assumes that the colors array size is `graphics_image_mpt`.
// The cache entry storage is limited (TILE_CACHE_SIZE) and circular. When full, the oldest cache entry will be reused.
// The first color is ignored, because it is the background color. Background is not drawn to the surface and so it isn't needed to store for comparison when searching for cached tile.
static SDL_Surface* graphics_tile_cache_new(term_data *td, char32_t index, uint32_t colors[]) {
	struct tile_cache_entry *entry;

	// Replace valid cache entries in FIFO order
	entry = &td->tile_cache[td->cache_position++];
	if (td->cache_position >= TILE_CACHE_SIZE) td->cache_position = 0;

	// Fill entry with values and clear surface.
	entry->index = index;
	entry->colors = memcpy(entry->colors, &colors[1], sizeof(uint32_t) * entry->ncolors);
	SDL_FillRect(entry->tile, NULL, 0x00000000);

	return(entry->tile);
}
 #endif

static void draw_colored_layers_to_surface(SDL_Rect dst_rect, SDL_Surface *dst, uint8_t n, SDL_Rect src_rect, SDL_Surface *layers[], SDL_Color colors[], SDL_Surface *preparation) {
	for (uint32_t i = 0; i < n; i++) {
		// Draw the mask of i-th layer in desired color to preparation surface.
		SDL_FillRect(preparation, NULL, SDL_MapRGBA(preparation->format, Pixel_quadruplet(colors[i])));
		SDL_BlitSurface(layers[i], &src_rect, preparation, NULL);

		// Draw preparation surface to destination.
		SDL_BlitSurface(preparation, NULL, dst, &dst_rect);
	}
}
static void term_data_draw_graphics_tile(term_data *td, int x, int y, char32_t index, SDL_Color colors[]) {
	//fprintf(stderr, "jezek - draw tile(x: %d, y: %d, index: %d)\n", x, y, index);
	SDL_Rect dst_rect;
	SDL_Surface *dst_surface;

 #ifdef TILE_CACHE_SIZE
	//fprintf(stderr, "jezek - try cache\n");
	dst_rect = (SDL_Rect){0, 0, td->fnt->wid, td->fnt->hgt};
	dst_surface = graphics_tile_cache_search(td, index, colors);
	if ( dst_surface != NULL) {
		//fprintf(stderr, "jezek - found cache\n");
		SDL_BlitSurface(dst_surface, NULL, td->win->surface, &(SDL_Rect){x, y, td->fnt->wid, td->fnt->hgt});
		return;
	}
	//fprintf(stderr, "jezek - add cache\n");
	dst_surface = graphics_tile_cache_new(td, index, colors);
 #else
	//fprintf(stderr, "jezek - no cache\n");
	dst_rect = (SDL_Rect){x, y, td->fnt->wid, td->fnt->hgt};
	dst_surface = td->win->surface;
 #endif
	uint32_t srcX = (index % graphics_image_tpr) * td->fnt->wid;
	uint32_t srcY = (index / graphics_image_tpr) * td->fnt->hgt;

	draw_colored_layers_to_surface(dst_rect, dst_surface, td->nlayers, (SDL_Rect){srcX, srcY, td->fnt->wid, td->fnt->hgt}, td->tiles_layers, &colors[1], td->tilePreparation);
 #ifdef TILE_CACHE_SIZE
	SDL_BlitSurface(dst_surface, NULL, td->win->surface, &(SDL_Rect){x, y, td->fnt->wid, td->fnt->hgt});
 #endif
}
// Draws a tile constructed from indexes using colors onto terminal surface coordinates.
// Assumes that length of 'indexes' == 'graphics_image_tpc' and length of 'colors' == 'graphics_image_tpc' * 'graphics_image_mpt' (and 'td->nlayers' == 'graphics_image_mpt' - 1).
// Assumes that colors are made for the 'td->win->surface->format' using SDL_MapRGBA function.
// The colors array contains colors that replaces mask for every index. Tho only background color is used is the background color for index 0. The background colors for every other index are ignored.
// Tiles with index 0xFFFFFFFF are skipped and not drawn.
static errr term_data_draw_graphics_tiles(term_data *td, int x, int y, char32_t *indexes, SDL_Color colors[]) {
	//fprintf(stderr, "jezek - draw tiles(x: %d, y: %d, indexes[0]: %d, indexes[1] %d)\n", x, y, indexes[0], indexes[1]);

	// Draw rectangle filled with the background color of the bottom tile onto the window.
	SDL_FillRect(td->win->surface, &(SDL_Rect){x, y, td->fnt->wid, td->fnt->hgt}, SDL_MapRGBA(td->win->surface->format, Pixel_quadruplet(colors[0])));

	for (uint32_t i = 0; i < graphics_image_tpc; i++) {
		if (indexes[i] == 0xFFFFFFFF) continue;
		if (y == 1 && indexes[0] == 50 && indexes[1] == 307 && i == 1) continue;
		term_data_draw_graphics_tile(td, x, y, indexes[i], &colors[i * graphics_image_mpt]);
	}
	return(0);
}

static bool Term_pict_sdl2_show_error = true;

/*
 * Draw some graphical characters.
 */
static errr Term_pict_sdl2(int x, int y, byte a, char32_t c) {

	if (graphics_image_mpt != 2 || graphics_image_tpc != 1) {
		if (Term_pict_sdl2_show_error) fprintf(stderr, "ERROR: Term_pict_sdl2: masks per tile %d or tiles per coord %d is wrong, should be %d, %d\n", graphics_image_mpt, graphics_image_tpc, 2, 1);
		Term_pict_sdl2_show_error = false;
		return(Infofnt_text_std(x, y, " ", 1));
	}

	term_data *td;

	// Catch use in chat instead of as feat attr or we crash.
	// Screen pad left check: In case it is used in the status bar for some reason.
	// Screen pad top check: Main screen top chat line or status line.
	if (Term && Term->data == &term_main && x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + SCREEN_WID && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + SCREEN_HGT) {
		flick_global_x = x;
		flick_global_y = y;
	} else flick_global_x = 0;

	a = term2attr(a);

 #ifndef EXTENDED_COLOURS_PALANIM
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x0F]);
  #else
	// Undefined case actually, we don't want to have a hole in the colour array (0..15 and then 32..32+x).
	Infoclr_set(clr[a & 0x1F]);
  #endif
 #else
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x1F]);
  #else
	Infoclr_set(clr[a & 0x3F]);
  #endif
 #endif

	if (Pixel_equal(Infoclr->fg, Infoclr->bg)) {
		// Foreground color is the same as background color. If this was text, the tile would be rendered as solid block of color.
		// But an image tile could contain some other color pixels and could result in no solid color tile. That's why paint a solid block as intended.
		return(Infofnt_text_std(x, y, " ", 1));
	}

	td = (term_data*)(Term->data);
	x *= Infofnt->wid;
	y *= Infofnt->hgt;

	term_data_draw_graphics_tiles(td, td->win->bw + x, td->win->bh + y, (char32_t[]){c - MAX_FONT_CHAR - 1}, (SDL_Color[]){Infoclr->bg, Infoclr->fg});
	return(0);
}

 #ifdef GRAPHICS_BG_MASK
static bool Term_pict_sdl2_2mask_show_error = true;

static errr Term_pict_sdl2_2mask(int x, int y, byte a, char32_t c, byte a_back, char32_t c_back) {

	if (graphics_image_mpt != 3 || graphics_image_tpc != 2) {
		if (Term_pict_sdl2_2mask_show_error) fprintf(stderr, "ERROR: Term_pict_sdl2_2mask: masks per tile %d or tiles per coordinate %d is wrong, should be %d, %d\n", graphics_image_mpt, graphics_image_tpc, 3, 2);
		Term_pict_sdl2_2mask_show_error = false;
		return(Term_pict_sdl2(x, y, a, c));
	}

	term_data *td;
	char32_t indexes[GRAPHICS_MAX_TPC] = {0};
	SDL_Color colors[GRAPHICS_MAX_MPT * GRAPHICS_MAX_TPC] = {0};

	// Catch use in chat instead of as feat attr, or we crash. :-s
	// (term-idx 0 is the main window; screen-pad-left check: In case it is used in the status bar for some reason; screen-pad-top checks: main screen top chat line or status line).
	if (Term && Term->data == &term_main && x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + SCREEN_WID && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + SCREEN_HGT) {
		flick_global_x = x;
		flick_global_y = y;
	} else flick_global_x = 0;

	/* Avoid visual glitches while not in 2mask mode */
	if (use_graphics != UG_2MASK) {
		a_back = TERM_DARK;
   #if 0
		c_back = 32; //space! NOT zero!
   #else
		c_back = Client_setup.f_char[FEAT_SOLID]; //'graphical space' for erasure
   #endif
	}

	// SPACE - Erase background, aka black background. This is for places where we have no bg-info, such as client-lore in knowledge menu.
	if (c_back == 32) a_back = TERM_DARK;

	a_back = term2attr(a_back);
 #ifndef EXTENDED_COLOURS_PALANIM
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a_back & 0x0F]);
  #else
	// Undefined case actually, we don't want to have a hole in the colour array (0..15 and then 32..32+x).
	Infoclr_set(clr[a_back & 0x1F]);
  #endif
 #else
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a_back & 0x1F]);
  #else
	Infoclr_set(clr[a_back & 0x3F]);
  #endif
 #endif
	if (c_back == 32 || c_back == 0) {
		// Background tile is an empty space and should not be painted.
		indexes[0] = 0xFFFFFFFF;
	} else {
		indexes[0] = c_back - MAX_FONT_CHAR - 1;
	}
	// Background color.
	colors[0] = Infoclr->bg;
	// Foreground color.
	colors[1] = Infoclr->fg;
	// Outline color. Set to background color.
	colors[2] = Infoclr->bg;

	a = term2attr(a);
 #ifndef EXTENDED_COLOURS_PALANIM
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x0F]);
  #else
	// Undefined case actually, we don't want to have a hole in the colour array (0..15 and then 32..32+x).
	Infoclr_set(clr[a & 0x1F]);
  #endif
 #else
  #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a & 0x1F]);
  #else
	Infoclr_set(clr[a & 0x3F]);
  #endif
 #endif
	indexes[1] = c - MAX_FONT_CHAR - 1;
	// Background color. For all tiles except bottom tile background color is ignored. Set to transparent black anyway.
	colors[graphics_image_mpt + 0] = (Pixel){0x00, 0x00, 0x00, 0x00};
	// Foreground color.
	colors[graphics_image_mpt + 1] = Infoclr->fg;
	// Outline color. Set to background color of the bottom tile.
	//TODO jezek - Try semi-transparent black for outline colors.
	colors[graphics_image_mpt + 2] = colors[0];

	if (Pixel_equal(Infoclr->fg, Infoclr->bg)) {
		// Foreground color is the same as background color. If this was text, the tile would be rendered as solid block of color.
		// But an image tile could contain some other color pixels and could result in no solid color tile. That's why paint a solid block as intended.
		return(Infofnt_text_std(x, y, " ", 1));
	}

	td = (term_data*)(Term->data);
	x *= Infofnt->wid;
	y *= Infofnt->hgt;

	return term_data_draw_graphics_tiles(td, td->win->bw + x, td->win->bh + y, indexes, colors);
}
 #endif /* GRAPHICS_BG_MASK */

#endif /* USE_GRAPHICS */

// Function scales the source surface `src` containing tiles with width `src_tile_wdt` and height `src_tile_hgt` pixels.
// Returned surface will contain scaled tiles with width `dst_tile_wdt` and height `dst_tile_hgt` pixels.
// Returned surface will have the same format as source surface.
// This function assumes that source surface width/height can be divided by source tile width/height and the surface pixel is a 32 bit value.
SDL_Surface *ScaleSurface(SDL_Surface *src, uint16_t src_tile_wdt, uint16_t src_tile_hgt, uint16_t dst_tile_wdt, uint16_t dst_tile_hgt) {
	if (!src || !src->format) return NULL;

	uint16_t new_wdt, new_hgt;
	int x, y, src_x, src_y;

	// Count of tiles horizontally and vertically and new scaled surface sizes.
	new_wdt = (src->w / src_tile_wdt) * dst_tile_wdt;
	new_hgt = (src->h / src_tile_hgt) * dst_tile_hgt;

	// Create returning surface.
	SDL_Surface *dst = SDL_CreateRGBSurface(0, new_wdt, new_hgt, 32, src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);
	if (!dst) {
		fprintf(stderr, "Error creating scaled surface: %s\n", SDL_GetError());
		return NULL;
	}

	// Lock surfaces.
	SDL_LockSurface(src);
	SDL_LockSurface(dst);

	// Fill scaled pixels.
	for (y = 0; y < new_hgt; ++y) {
		src_y = (y * src->h) / new_hgt;
		for (x = 0; x < new_wdt; ++x) {
			src_x = (x * src->w) / new_wdt;

			// Assumes a pixel is 32 bit wide.
			((uint32_t*)dst->pixels)[y * new_wdt + x] = ((uint32_t*)src->pixels)[src_y * src->w + src_x];
		}
	}

	// Unlock surfaces.
	SDL_UnlockSurface(dst);
	SDL_UnlockSurface(src);

	return dst;
}

// Returns RGBA.
static uint32_t graphics_default_mask(uint8_t n) {
	if (graphics_image_mpt == 2) {
		switch (n) {
			case 0: return (GFXMASK_BG_R << 24) | (GFXMASK_BG_G << 16) | (GFXMASK_BG_B << 8) | 0xFF;
			case 1: return (GFXMASK_FG_R << 24) | (GFXMASK_FG_G << 16) | (GFXMASK_FG_B << 8) | 0xFF;
		}
	}
	else if (graphics_image_mpt == 3) {
		switch (n) {
			case 0: return (GFXMASK_BG2_R << 24) | (GFXMASK_BG2_G << 16) | (GFXMASK_BG2_B << 8) | 0xFF;
			case 1: return (GFXMASK_FG_R << 24) | (GFXMASK_FG_G << 16) | (GFXMASK_FG_B << 8) | 0xFF;
			case 2: return (GFXMASK_FG_R << 24) | (GFXMASK_FG_G << 16) | (GFXMASK_FG_B << 8) | 0xFF;
		}
	}
	fprintf(stderr, "Warning: Color for mask no %d, when there are %d masks per tile is undefined!\n", n, graphics_image_mpt);
	return(0);
};

// Initializes graphics stuff and prepare surfaces for a terminal's term_data from graphics_image.
// Frees all graphics resources allocated before and then makes the initialization.
static void term_data_init_graphics(term_data *td) {
	// Resize tiles and separate into layers by mask colors.
	// First layer is the image with all other mask colors, except foreground color, made fully transparent black.
	// Other layers are just the other mask color pixels on transparent background.
	// The background mask color does not need to be extracted to a layer.

	// Free old tiles.
	free_graphics(td);

	// Resize the loaded graphics image, so tile size match font size.
	SDL_Surface *scaled_image;

	scaled_image = ScaleSurface(graphics_image, graphics_tile_wid, graphics_tile_hgt, td->fnt->wid, td->fnt->hgt);

	// Initialize surfaces for all layers.
	td->nlayers = graphics_image_mpt - 1;
	C_MAKE(td->tiles_layers, td->nlayers, SDL_Surface*);
	for (int i = 0; i < td->nlayers; i++) {
		td->tiles_layers[i] = SDL_CreateRGBSurfaceWithFormat(0, scaled_image->w, scaled_image->h, 32, SDL_PIXELFORMAT_RGBA32);
		// The transparent color has to be different from mask color (color key is only RGB, alpha is not considered).
		SDL_FillRect(td->tiles_layers[i], NULL, SDL_MapRGBA(td->tiles_layers[i]->format, ((graphics_image_masks_colors[i+1] >> 24) & 0xFF) ^ 0xFF, ((graphics_image_masks_colors[i+1] >> 16) & 0XFF) ^ 0xFF, ((graphics_image_masks_colors[i+1] >> 8) & 0xFF) ^ 0xFF, 0x00));
		SDL_SetColorKey(td->tiles_layers[i], SDL_TRUE, SDL_MapRGB(td->tiles_layers[i]->format, ((graphics_image_masks_colors[i+1] >> 24) & 0xFF), ((graphics_image_masks_colors[i+1] >> 16) & 0xFF), ((graphics_image_masks_colors[i+1] >> 8) & 0xFF)));
		SDL_SetSurfaceBlendMode(td->tiles_layers[i], SDL_BLENDMODE_NONE);
	}

	// Create first layer by making background color fully transparent black.
	SDL_SetColorKey(scaled_image, SDL_TRUE, SDL_MapRGB(scaled_image->format, ((graphics_image_masks_colors[0] >> 24) & 0xFF), ((graphics_image_masks_colors[0] >> 16) & 0xFF), ((graphics_image_masks_colors[0] >> 8) & 0xFF)));
	SDL_BlitSurface(scaled_image, NULL, td->tiles_layers[0], NULL);

	// The scaled image is not needed anymore.
	SDL_FreeSurface(scaled_image);

	// If there are more layers, separate each mask color to it's own layer surface.
	if (td->nlayers > 1) {
		uint32_t* srcPixels = (uint32_t*)td->tiles_layers[0]->pixels;
		for (int y = 0; y < td->tiles_layers[0]->h; y++) {
			for (int x = 0; x < td->tiles_layers[0]->w; x++) {
				uint32_t pos = (y * td->tiles_layers[0]->w) + x;
				for (int i = 1; i < td->nlayers; i++) {
					if (srcPixels[pos] == SDL_MapRGBA(td->tiles_layers[0]->format, ((graphics_image_masks_colors[i+1] >> 24) & 0xFF), ((graphics_image_masks_colors[i+1] >> 16) & 0xFF), ((graphics_image_masks_colors[i+1] >> 8) & 0xFF), 0xFF)) {
						uint32_t* dstPixels = (uint32_t*)td->tiles_layers[i]->pixels;
						dstPixels[pos] = srcPixels[pos];
						srcPixels[pos] = SDL_MapRGBA(td->tiles_layers[0]->format, ((graphics_image_masks_colors[1] >> 24) & 0xFF) ^ 0xFF, ((graphics_image_masks_colors[1] >> 16) & 0xFF) ^ 0xFF, ((graphics_image_masks_colors[1] >> 8) & 0xFF) ^ 0xFF, 0);
					}
				}
			}
		}
	}

	// Initialize preparation surface.
	td->tilePreparation = SDL_CreateRGBSurfaceWithFormat(0, td->fnt->wid, td->fnt->hgt, 32, SDL_PIXELFORMAT_RGBA32);
	SDL_SetSurfaceBlendMode(td->tilePreparation, SDL_BLENDMODE_BLEND);

	// Note: If we want to cache even more graphics for faster drawing, we could initialize 16 copies of the graphics image with all possible mask colors already applied.
	// Memory cost could become "large" quickly though (eg 5MB bitmap -> 80MB). Not a real issue probably.
#ifdef TILE_CACHE_SIZE
	for (int i = 0; i < TILE_CACHE_SIZE; i++) {
		td->tile_cache[i].tile = SDL_CreateRGBSurfaceWithFormat(0, td->fnt->wid, td->fnt->hgt, 32, SDL_PIXELFORMAT_RGBA32);
		SDL_SetSurfaceBlendMode(td->tile_cache[i].tile, SDL_BLENDMODE_BLEND);
		td->tile_cache[i].ncolors = graphics_image_mpt - 1;
		C_MAKE(td->tile_cache[i].colors, td->tile_cache[i].ncolors, uint32_t);
	}
#endif
}

/*
 * Initialize a term_data
 */
static errr term_data_init(int index, term_data *td, bool fixed, cptr name, cptr font) {
	//fprintf(stderr, "jezek - term_data_init(index: %d, *td: %p, fixed: %b, name: \"%s\", font: \"%s\")\n", index, td, fixed, name, font);
	/* Use values from .tomenetrc;
	   Environment variables (see further below) may override those. */
	int win_cols = term_prefs[index].columns;
	int win_lines = term_prefs[index].lines;
	int topx = term_prefs[index].x;
	int topy = term_prefs[index].y;

	//fprintf(stderr, "jezek -  term_data_init: initialize font %s\n", font);
	/* Prepare the standard font */
	MAKE(td->fnt, infofnt);
	infofnt *old_infofnt = Infofnt;
	Infofnt_set(td->fnt);
	if (Infofnt_init(font) == -1) {
		/* Initialization failed, log and try to use the default font. */
		fprintf(stderr, "Failed to load the \"%s\" font for terminal %d\n", font, index);
		if (in_game) {
			/* If in game, inform the user. */
			Infofnt_set(old_infofnt);
			plog_fmt("Failed to load the \"%s\" font! Falling back to default font.\n", font);
			Infofnt_set(td->fnt);
		} 
		if (Infofnt_init(sdl2_terms_font_default[index]) == -1) {
			/* Initialization of the default font failed too. Log, free allocated memory and return with error. */
			fprintf(stderr, "Failed to load the default \"%s\" font for terminal %d\n", sdl2_terms_font_default[index], index);
			Infofnt_set(old_infofnt);
			if (in_game) {
				/* If in game, inform the user. */
				plog_fmt("Failed to load the default \"%s\" font too! Try to change font manually.\n", sdl2_terms_font_default[index]);
			}
			FREE(td->fnt, infofnt);
			return(1);
		}
	}
	//fprintf(stderr, "jezek -  term_data_init: font initialized\n");



	/* Extract widths and heights from env variables. */
	cptr n;
	for (int i = 0; i < ANGBAND_TERM_MAX; ++i) {
		if (!strcmp(name, ang_term_name[i])) {
			n = getenv(sdl2_terms_wid_env[i]);
			if (n) win_cols = atoi(n);
			n = getenv(sdl2_terms_hgt_env[i]);
			if (n) win_lines = atoi(n);
			break;
		}
	}

	/* Hack -- extract key buffer size */
	int num = (fixed ? 1024 : 16);

	/* Reset timers just to be sure. */
	td->resize_timer = 0;

	// Calculate window and drawing field sizes.
	int wid_draw = win_cols * td->fnt->wid;
	int hgt_draw = win_lines * td->fnt->hgt;
	int wid_border = wid_draw + (2 * DEFAULT_SDL2_BORDER_WIDTH);
	int hgt_border = hgt_draw + (2 * DEFAULT_SDL2_BORDER_WIDTH);

	MAKE(td->win, infowin);
	Infowin_set(td->win);

	Infowin_init(topx, topy, wid_border, hgt_border, DEFAULT_SDL2_BORDER_WIDTH, color_default_b, color_default_bg);

	if (!strcmp(name, ang_term_name[0])) {
		char version[MAX_CHARS];
		sprintf(version, "TomeNET %d.%d.%d%s",
		    VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, CLIENT_VERSION_TAG);
		Infowin_set_name(version);
	} else Infowin_set_name(name);

#ifdef USE_GRAPHICS
	/* No graphics yet */
	td->nlayers = 0;
	td->tiles_layers = NULL;
	td->tilePreparation = NULL;
 #ifdef TILE_CACHE_SIZE
	for (int i = 0; i < TILE_CACHE_SIZE; i++) {
		td->tile_cache[i].tile = NULL;
		td->tile_cache[i].index = 0;
		td->tile_cache[i].ncolors = 0;
		td->tile_cache[i].colors = NULL;
	}
 #endif
#endif /* USE_GRAPHICS */

	term *t = &td->t;

	/* Initialize the term (full size) */
	term_init(t, win_cols, win_lines, num);

	/* Use a "soft" cursor */
	t->soft_cursor = TRUE;

	/* Erase with "white space" */
	t->attr_blank = TERM_WHITE;
	t->char_blank = ' ';

	/* Hooks */
	t->xtra_hook = Term_xtra_sdl2;
	t->curs_hook = Term_curs_sdl2;
	t->wipe_hook = Term_wipe_sdl2;
	t->text_hook = Term_text_sdl2;
	t->nuke_hook = Term_nuke_sdl2;

//#ifdef USE_GRAPHICS
//
//	/* Use graphics */
//	if (use_graphics) {
//		term_data_init_graphics(td);
//
//		if (td->tiles_layers != NULL && td->tilePreparation != NULL) {
//			/* Graphics hook */
// #ifdef GRAPHICS_BG_MASK
//			if (use_graphics == UG_2MASK) {
//				t->pict_hook_2mask = Term_pict_sdl2_2mask;
//			}
// #endif
//			t->pict_hook = Term_pict_sdl2;
//
//			/* Use graphics sometimes */
//			t->higher_pict = TRUE;
//		}
//		else {
//			fprintf(stderr, "Couldn't prepare images for terminal %d\n", index);
//		}
//	}
//
//#endif /* USE_GRAPHICS */

	/* Save the data */
	t->data = td;

	/* Activate (important) */
	Term_activate(t);

	/* Success */
	return(0);
}

/*
 * Names of the 16 colors
 *   Black, White, Slate, Orange,    Red, Green, Blue, Umber
 *   D-Gray, L-Gray, Violet, Yellow, L-Red, L-Green, L-Blue, L-Umber
 *
 * Colors courtesy of: Torbj|rn Lindgren <tl@ae.chalmers.se>
 *
 * These colors are overwritten with the generic, OS-independant client_color_map[] in enable_common_colormap_sdl2()!
 */
static char color_name[CLIENT_PALETTE_SIZE][8] = {
	"#000000",      /*  0 - BLACK */
	"#ffffff",      /*  1 - WHITE */
	"#9d9d9d",      /*  2 - GRAY */
	"#ff8d00",      /*  3 - ORANGE */
	"#b70000",      /*  4 - RED */
	"#009d44",      /*  5 - GREEN */
#ifndef READABILITY_BLUE
	"#0000ff",      /*  6 - BLUE */
#else
	"#0033ff",      /*  6 - BLUE */
#endif
	"#8d6600",      /*  7 - BROWN (UMBER) */
#ifndef DISTINCT_DARK
	"#747474",      /*  8 - DARKGRAY */
#else
	//"#585858",      /*  8 - DARKGRAY */
	"#666666",      /*  8 - DARKGRAY */
#endif
	"#cdcdcd",      /*  9 - LIGHTGRAY */
	"#af00ff",      /* 10 - PURPLE */
	"#ffff00",      /* 11 - YELLOW */
	"#ff3030",      /* 12 - PINK */
	"#00ff00",      /* 13 - LIGHTGREEN */
	"#00ffff",      /* 14 - LIGHTBLUE */
	"#c79d55",      /* 15 - LIGHTBROWN */
#ifdef EXTENDED_COLOURS_PALANIM
	/* And clone the above 16 standard colors again here: */
	"#000000",      /* 16 - BLACK */
	"#ffffff",      /* 17 - WHITE */
	"#9d9d9d",      /* 18 - GRAY */
	"#ff8d00",      /* 19 - ORANGE */
	"#b70000",      /* 20 - RED */
	"#009d44",      /* 21 - GREEN */
 #ifndef READABILITY_BLUE
	"#0000ff",      /* 22 - BLUE */
 #else
	"#0033ff",      /* 22 - BLUE */
 #endif
	"#8d6600",      /* 23 - BROWN */
 #ifndef DISTINCT_DARK
	"#747474",      /* 24 - DARKGRAY */
 #else                     
	//"#585858",      /* 24 - DARKGRAY */
	"#666666",      /* 24 - DARKGRAY */
 #endif
	"#cdcdcd",      /* 25 - LIGHTGRAY */
	"#af00ff",      /* 26 - PURPLE */
	"#ffff00",      /* 27 - YELLOW */
	"#ff3030",      /* 28 - PINK */
	"#00ff00",      /* 29 - LIGHTGREEN */
	"#00ffff",      /* 30 - LIGHTBLUE */
	"#c79d55",      /* 31 - LIGHTBROWN */
#endif
};
#ifdef EXTENDED_BG_COLOURS
 /* Format: (fg, bg) */
 static char color_ext_name[TERMX_AMT][2][8] = {
	//{"#0000ff", "#444444", },
	//{"#ffffff", "#0000ff", },
	//{"#666666", "#0000ff", },
	{"#aaaaaa", "#112288", },	/* TERMX_BLUE */
	{"#aaaaaa", "#007700", },	/* TERMX_GREEN */
	{"#aaaaaa", "#770000", },	/* TERMX_RED */
	{"#aaaaaa", "#AAAA00", },	/* TERMX_YELLOW */
	{"#aaaaaa", "#555555", },	/* TERMX_GREY */
	{"#aaaaaa", "#BBBBBB", },	/* TERMX_WHITE */
	{"#aaaaaa", "#333388", },	/* TERMX_PURPLE */
};
#endif

static void enable_common_colormap_sdl2() {
	int i;
	unsigned long c;
#ifdef EXTENDED_BG_COLOURS
	unsigned long b;
#endif

	for (i = 0; i < CLIENT_PALETTE_SIZE; i++) {
		c = client_color_map[i];

		sprintf(color_name[i], "#%06lx", c & 0xFFFFFFL);
	}

#ifdef EXTENDED_BG_COLOURS
	for (i = 0; i < TERMX_AMT; i++) {
		c = client_ext_color_map[i][0];
		b = client_ext_color_map[i][1];

		sprintf(color_ext_name[i][0], "#%06lx", c & 0xFFFFFFL);
		sprintf(color_ext_name[i][1], "#%06lx", b & 0xFFFFFFL);
	}
#endif
}

void enable_readability_blue_sdl2(void) {
	/* New colour code */
	client_color_map[6] = 0x0033FF;
#ifdef EXTENDED_COLOURS_PALANIM
	client_color_map[BASE_PALETTE_SIZE + 6] = 0x0033FF;
#endif
}

static term_data* term_idx_to_term_data(int term_idx) {
	term_data *td = &term_main;

	switch (term_idx) {
	case 0: td = &term_main; break;
	case 1: td = &term_1; break;
	case 2: td = &term_2; break;
	case 3: td = &term_3; break;
	case 4: td = &term_4; break;
	case 5: td = &term_5; break;
	case 6: td = &term_6; break;
	case 7: td = &term_7; break;
	case 8: td = &term_8; break;
	case 9: td = &term_9; break;
	}

	return(td);
}

static int term_data_to_term_idx(term_data *td) {
	if (td == &term_main) return(0);
	if (td == &term_1) return(1);
	if (td == &term_2) return(2);
	if (td == &term_3) return(3);
	if (td == &term_4) return(4);
	if (td == &term_5) return(5);
	if (td == &term_6) return(6);
	if (td == &term_7) return(7);
	if (td == &term_8) return(8);
	if (td == &term_9) return(9);
	return(-1);
}

/* 
 * If provided font is a .ttf and size is missing or out of bounds, add or fix the font size.
 */
static void validate_font_format(char *font, int term_idx) {
	char font_base[256];
	int8_t size = 0;
	if (is_ttf_font(font, font_base, sizeof(font_base), &size)) {
		if (size < 0) size = sdl2_terms_ttf_size_default[term_idx];
		if (size < MIN_SDL2_TTF_FONT_SIZE) size = MIN_SDL2_TTF_FONT_SIZE;
		if (size > MAX_SDL2_TTF_FONT_SIZE) size = MAX_SDL2_TTF_FONT_SIZE;
		snprintf(font, 256, "%s %d", font_base, size);
	}
}

/*
 * Initialization of i-th SDL2 terminal window.
 */
static errr sdl2_term_init(int term_id) {
	cptr fnt_name;
	char valid_fnt_name[256];
	errr err;

	if (term_id < 0 || term_id >= ANGBAND_TERM_MAX) {
		fprintf(stderr, "Terminal index %d out of bounds\n", term_id);
		return(1);
	}

	if (ang_term[term_id]) {
		fprintf(stderr, "Terminal window with index %d is already initialized\n", term_id);
		/* Success. */
		return(0);
	}

	/* Check environment for SDL2 terminal font. */
	fnt_name = getenv(sdl2_terms_font_env[term_id]);
	/* Check environment for "base" font. */
	if (!fnt_name) fnt_name = getenv("TOMENET_SDL2_FONT");
	/* Use loaded (from config file) or predefined default font. */
	if (!fnt_name && strlen(term_prefs[term_id].font)) fnt_name = term_prefs[term_id].font;
	/* Paranoia, use the default. */
	if (!fnt_name) fnt_name = sdl2_terms_font_default[term_id];

	strncpy(valid_fnt_name, fnt_name, sizeof(valid_fnt_name));
	valid_fnt_name[sizeof(valid_fnt_name)-1] = '\0';
	validate_font_format(valid_fnt_name, term_id);

	/* Initialize the terminal window, allow resizing, for font changes. */
	err = term_data_init(term_id, sdl2_terms_term_data[term_id], FALSE, ang_term_name[term_id], valid_fnt_name);
	/* Store created terminal with SDL2 term data to ang_term array, even if term_data_init failed, but only if there is one. */
	if (Term && term_data_to_term_idx(Term->data) == term_id) ang_term[term_id] = Term;

	if (err) {
		fprintf(stderr, "Error initializing term_data for SDL2 terminal with index %d\n", term_id);
		if (ang_term[term_id]) {
			term_nuke(ang_term[term_id]);
			ang_term[term_id] = NULL;
		}
		return(err);
	}

	/* Success. */
	return(0);
}

#ifdef USE_GRAPHICS
errr init_graphics_sdl2(void) {
	char filename[1024];
	char path[1024];
	//char *data = NULL;
	//errr rerr = 0;
	//int depth, x, y;

	/* Load graphics file. Quit if file missing or load error. */

	/* Check for tiles string & extract tiles width & height. */
	if (2 != sscanf(graphic_tiles, "%hux%hu", &graphics_tile_wid, &graphics_tile_hgt)) {
		sprintf(use_graphics_errstr, "Couldn't extract tile dimensions from: %s", graphic_tiles);
		return(101);
	}

	if (graphics_tile_wid <= 0 || graphics_tile_hgt <= 0) {
		sprintf(use_graphics_errstr, "Invalid tiles dimensions: %dx%d", graphics_tile_wid, graphics_tile_hgt);
		return(102);
	}

	/* Build & allocate the graphics path. */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "graphics");
	ANGBAND_DIR_XTRA_GRAPHICS = string_make(path);

	/* Build the name of the graphics file. */
	path_build(filename, 1024, ANGBAND_DIR_XTRA_GRAPHICS, graphic_tiles);
	strcat(filename, ".bmp");

	/* Load .bmp image. */
	graphics_image = SDL_LoadBMP(filename);
	if (graphics_image == NULL) {
		sprintf(use_graphics_errstr, "Graphics file \"%s\" SDL_LoadBMP error: %s\n", filename, SDL_GetError());
		return(103);
	}

	// Check, if format is RGBA32 and convert if not.
	if (graphics_image->format->format != SDL_PIXELFORMAT_RGBA32) {
		fprintf(stderr, "jezek - converting to RGBA32\n");
		SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(graphics_image, SDL_PIXELFORMAT_RGBA32, 0);
		if (!convertedSurface) {
			sprintf(use_graphics_errstr, "Error converting loaded image into RGBA32 format: %s\n", SDL_GetError());
			SDL_FreeSurface(graphics_image);
			return(104);
		}
		SDL_FreeSurface(graphics_image);
		graphics_image = convertedSurface;
	}

	/* Ensure the BMP isn't empty or too small */
	if (graphics_image->w < graphics_tile_wid || graphics_image->h < graphics_tile_hgt) {
		sprintf(use_graphics_errstr, "Invalid image dimensions (width x height): %dx%d", graphics_image->w, graphics_image->h);
		return(105);
	}

	/* Calculate tiles per row. */
	graphics_image_tpr = graphics_image->w / graphics_tile_wid;
	if (graphics_image_tpr <= 0) { /* Paranoia. */
		sprintf(use_graphics_errstr, "Invalid image tiles per row count: %d", graphics_image_tpr);
		return(106);
	}

	// Prepare masks colors array and other mask related variables.
	graphics_image_mpt = 2;
	graphics_image_tpc = 1;
 #ifdef GRAPHICS_BG_MASK
	if (use_graphics == UG_2MASK) {
		graphics_image_mpt = 3;
		graphics_image_tpc = 2;
	}
 #endif
	fprintf(stderr, "jezek - masks per tile: %u\n", graphics_image_mpt);

	graphics_reinitialize = true;

	return(0);
}
#endif
/*
 * Initialization function for an "SDL2" module to Angband
 */
errr init_sdl2(void) {
	SDL_DisplayMode dm;
	SDL_PixelFormat* pixel_format;
	bool dpy_color;
	int i;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "ERROR: init_sdl2: Video SDL_Init error: %s\n", SDL_GetError());
		return(-1);
	}

       if (TTF_Init() != 0) {
               fprintf(stderr, "ERROR: init_sdl2: TTF_Init error: %s\n", TTF_GetError());
               SDL_Quit();
               return(-1);
       }

       if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
               fprintf(stderr, "ERROR: init_sdl2: IMG_Init error: %s\n", IMG_GetError());
               TTF_Quit();
               SDL_Quit();
               return(-1);
       }

	/* Initialize SDL_net */
	if (SDLNet_Init() < 0) {
		fprintf(stderr, "ERROR: init_sdl2: SDLNet_Init error: %s\n", SDLNet_GetError());
		SDL_Quit();
		return 1;
	}

	if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
		fprintf(stderr, "ERROR: init_sdl2: SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
		TTF_Quit();
		SDL_Quit();
		return(-1);
	}

	pixel_format = SDL_AllocFormat(dm.format);
	if (pixel_format) {
		dpy_color = ((pixel_format->BitsPerPixel > 1) ? 1 : 0);
		SDL_FreeFormat(pixel_format);
	}

#ifdef CUSTOMIZE_COLOURS_FREELY
	/* Actually use fg-colour of index #0 (usually black) as the bg colour. */
	color_default_bg = (Pixel){
		.r = (client_color_map[0] >> 16 ) & 0xFF,
		.g = (client_color_map[0] >> 8 ) & 0xFF,
		.b = client_color_map[0] & 0xFF,
		.a = 0xFF
	};
#endif

	/* set OS-specific resize_main_window() hook */
	resize_main_window = resize_main_window_sdl2;

	enable_common_colormap_sdl2();

	/* Prepare color "xor" (for cursor) */
	MAKE(xor, infoclr);
	Infoclr_set (xor);
	Infoclr_init_parse("fg", "bg", true);
	//fprintf(stderr, "jezek - xor->fg: %x, xor->bg: %x, xor->xor: %b\n", xor->fg, xor->bg, xor->xor);

	/* Prepare the colors (including "black") */
	for (i = 0; i < CLIENT_PALETTE_SIZE; ++i) {
		cptr cname = color_name[0];

		MAKE(clr[i], infoclr);
		Infoclr_set(clr[i]);
		if (dpy_color) cname = color_name[i];
		else if (i) cname = color_name[1];
		Infoclr_init_parse(cname, "bg", false);
	}

#ifdef EXTENDED_BG_COLOURS
	/* Prepare the extended background-using colors */
	for (i = 0; i < TERMX_AMT; ++i) {
		cptr cname = color_name[0], cname2 = color_name[0];

		MAKE(clr[CLIENT_PALETTE_SIZE + i], infoclr);
		Infoclr_set(clr[CLIENT_PALETTE_SIZE + i]);
		if (dpy_color) {
			cname = color_ext_name[i][0];
			cname2 = color_ext_name[i][1];
		}
		Infoclr_init_parse(cname, cname2, false);
	}
#endif



	/* Initialize paths, to get access to lib directories. */
	init_stuff();

	char path[1024];
	// Build the "font" path
	path_build(path, 1024, ANGBAND_DIR_XTRA, "font");
	ANGBAND_DIR_XTRA_FONT = string_make(path);
	//fprintf(stderr, "jezek - init_sdl2: ANGBAND_DIR_XTRA_FONT: %s\n", ANGBAND_DIR_XTRA_FONT);

#ifdef USE_GRAPHICS
	fprintf(stderr, "jezek - use_graphics: %u\n", use_graphics);
	if (use_graphics) {
		use_graphics_err = init_graphics_sdl2();
		if (use_graphics_err != 0) {
 #ifndef GFXERR_FALLBACK
			quit_fmt("Graphics load error (%d): %s\n", use_graphics_err, use_graphics_errstr);
 #else
			fprintf(stderr, "Graphics load error (%d): %s\n", use_graphics_err, use_graphics_errstr);
			printf("Disabling graphics and falling back to normal text mode.\n");
			use_graphics = 0;
			/* Actually also show it as 'off' in =g menu, as in, "desired at config-file level" */
			use_graphics_new = false;
 #endif
		}
	}
#endif /* USE_GRAPHICS */

	/* Initialize each term */
	/* When run trough WINE, the main window doesn't rise above all others. Initialize the main window last. */
	for (i = 1; i < ANGBAND_TERM_MAX; i++) {
		/* Visibility depends on configuration. */
		if (term_prefs[i].visible) {
			if (sdl2_term_init(i) != 0) {
				fprintf(stderr, "Error initializing SDL2 terminal window with index %d\n", i);
			}
		}
	}
	if (sdl2_term_init(0) != 0) {
		fprintf(stderr, "Error initializing SDL2 main terminal window\n");
		/* Can't run without main screen. */
		return(1);
	}


	/* Activate the "Angband" main window screen. */
	Term_activate(&term_main.t);

	/* Raise the "Angband" main window. */
	Infowin_set(term_main.win);
	Infowin_set_focus();
	Infowin_raise();

	//TODO jezek - Remove at the end.
	//PCF_Font *font = (PCF_Font*)(term_main.fnt->font);
	//if (font->bitmap != NULL) {
	//	SDL_FillRect(Infowin->surface, &(SDL_Rect){Infowin->bw, Infowin->bh, font->glyphWidth, font->glyphHeight*2}, SDL_MapRGBA(Infowin->surface->format, Pixel_quadruplet(0x000088FF)));
	//	SDL_Surface *sur = PCF_RenderText(font, "#", (SDL_Color){Pixel_quadruplet(0x008800FF)}, (SDL_Color){Pixel_quadruplet(0x880000FF)});
	//	SDL_SetSurfaceBlendMode(sur, SDL_BLENDMODE_NONE);
	//	SDL_BlitSurface(sur, NULL, Infowin->surface, &(SDL_Rect){Infowin->bw, Infowin->bh, font->glyphWidth, font->glyphHeight});
	//	SDL_UpdateWindowSurface(Infowin->window);
	//	getchar();
	//}

	//if (term_main.nlayers > 0) {
	//	for (int i = 0; i < term_main.nlayers; i++) {
	//		SDL_FillRect(term_main.win->surface, &(SDL_Rect){term_main.win->bw, term_main.win->bh + (i*2*term_main.fnt->hgt) + (term_main.fnt->hgt / 2), 8 * term_main.fnt->wid, term_main.fnt->hgt}, SDL_MapRGBA(term_main.win->surface->format, Pixel_quadruplet(0x888888FF)));
	//		SDL_BlitSurface(term_main.tiles_layers[i], &(SDL_Rect){0, 0, 8 * term_main.fnt->wid, term_main.fnt->hgt}, term_main.win->surface, &(SDL_Rect){term_main.win->bw, term_main.win->bh + (i*2*term_main.fnt->hgt), 8 * term_main.fnt->wid, term_main.fnt->hgt});
	//	}
	//	SDL_UpdateWindowSurface(term_main.win->window);
	//	getchar();
	//}

	//if (term_main.nlayers > 0) {
	//	for (int i = 0; i < 8; i++) {
	//		term_data_draw_graphics_tile(&term_main, term_main.win->bw + (term_main.fnt->wid * i), term_main.win->bh, (char32_t[]){i}, (uint32_t[]){0xFF0000FF, 0x00FF00FF});
	//	}
	//	SDL_UpdateWindowSurface(term_main.win->window);
	//	getchar();
	//}
	//if (term_main.nlayers > 0) {
	//	char32_t indexes[] = {50, 307, 50, 307, 3309, 50, 3309, 50};
	//	uint32_t colors[] = {0x00FF00FF, 0xFF0000FF, 0x0000FFFF, 0x00FF00FF, 0xFF0000FF, 0x0000FFFF};
	//	//uint32_t colors[] = {0x00FF00FF, 0xFF0000FF, 0x00FF00FF, 0x00FF00FF, 0xFF0000FF, 0x00FF00FF};
	//	char32_t tiles[] = {0, 0};
	//	for (int i = 0; i < 2; i++) {
	//		tiles[1] = indexes[i];
	//		if (i == 3 || i == 4) {
	//			tiles[0] = 50; 
	//		} else {
	//			tiles[0] = 0xFFFFFFFF;
	//		}
	//		term_data_draw_graphics_tiles(&term_main, term_main.win->bw + (term_main.fnt->wid * i), term_main.win->bh, tiles, colors);
	//		term_data_draw_graphics_tiles(&term_main, term_main.win->bw + (term_main.fnt->wid * i), term_main.win->bh + term_main.fnt->hgt, tiles, colors);
	//	
	//		SDL_Rect src_rect = {((indexes[i]) % graphics_image_tpr) * graphics_tile_wid, ((indexes[i]) / graphics_image_tpr) * graphics_tile_hgt, graphics_tile_wid, graphics_tile_hgt};
	//		SDL_Rect dst_rect = {term_main.win->bw + (graphics_tile_wid * i), term_main.win->bh + term_main.fnt->hgt * 3, graphics_tile_wid, graphics_tile_hgt};
	//		SDL_FillRect(term_main.win->surface, &dst_rect, SDL_MapRGBA(term_main.win->surface->format, 0, 255, 0, 255));
	//		SDL_BlitSurface(graphics_image, &src_rect, term_main.win->surface, &dst_rect);
	//		dst_rect.y += graphics_tile_hgt;
	//		SDL_FillRect(term_main.win->surface, &dst_rect, SDL_MapRGBA(term_main.win->surface->format, 0, 255, 0, 255));
	//		SDL_BlitSurface(graphics_image, &src_rect, term_main.win->surface, &dst_rect);

	//		src_rect = (SDL_Rect){((indexes[i]) % graphics_image_tpr) * term_main.fnt->wid, ((indexes[i]) / graphics_image_tpr) * term_main.fnt->hgt, term_main.fnt->wid, term_main.fnt->hgt};
	//		dst_rect = (SDL_Rect){term_main.win->bw + (term_main.fnt->wid * i), term_main.win->bh + term_main.fnt->hgt * 6, term_main.fnt->wid, term_main.fnt->hgt};
	//		SDL_FillRect(term_main.win->surface, &dst_rect, SDL_MapRGBA(term_main.win->surface->format, 0, 255, 0, 255));
	//		draw_colored_layers_to_surface(dst_rect, term_main.win->surface, term_main.nlayers, src_rect, term_main.tiles_layers, &colors[1], term_main.tilePreparation);
	//		dst_rect.y += term_main.fnt->hgt;
	//		SDL_FillRect(term_main.win->surface, &dst_rect, SDL_MapRGBA(term_main.win->surface->format, 0, 255, 0, 255));
	//		draw_colored_layers_to_surface(dst_rect, term_main.win->surface, term_main.nlayers, src_rect, term_main.tiles_layers, &colors[1], term_main.tilePreparation);
	//	}
	//	SDL_UpdateWindowSurface(term_main.win->window);
	//	getchar();
	//}
//jezek : pict2mask(x: 34, y: 28, a: 1, c: 563, a_back: 53, c_back: 306)
//jezek - draw(x: 545, y: 617, indexes[0]: 50, indexes[1] 307)
//jezek - draw: indexes[0]: 50
//jezek - draw: indexes[1]: 307
//jezek : pict2mask(x: 35, y: 28, a: 1, c: 3565, a_back: 53, c_back: 306)
//jezek - draw(x: 561, y: 617, indexes[0]: 50, indexes[1] 3309)
//jezek - draw: indexes[0]: 50
//jezek - draw: indexes[1]: 3309
	/* Success */
	return(0);
}

static void term_force_font(int term_idx, cptr fnt_name);

/* EXPERIMENTAL // allow user to change main window font live - C. Blue
 * So far only uses 1 parm ('s') to switch between hardcoded choices:
 * -1 - cycle
 *  0 - normal
 *  1 - big
 *  2 - bigger
 *  3 - huge */
void change_font(int s) {
	/* use main window font for measuring */
	char tmp[128] = "";

	if (term_main.fnt->name) strcpy(tmp, term_main.fnt->name);
	else strcpy(tmp, DEFAULT_SDL2_FONT);

	/* cycle? */
	if (s == -1) {
		if (strstr(tmp, " 10")) s = 1;
		else if (strstr(tmp, " 12")) s = 2;
		else if (strstr(tmp, " 14")) s = 3;
		else if (strstr(tmp, " 16")) s = 0;
	}

	//TODO jezek - Just do cycling, don't change font names.
	/* Force the font */
	switch (s) {
	case 0:
		/* change main window font */
		term_force_font(0, "Hack-Regular.ttf 10");
		/* Change sub windows too */
		term_force_font(1, "Hack-Regular.ttf 10");
		term_force_font(2, "Hack-Regular.ttf 10");
		term_force_font(3, "Hack-Regular.ttf 6");
		term_force_font(4, "Hack-Regular.ttf 8");
		term_force_font(5, "Hack-Regular.ttf 8");
		term_force_font(6, "Hack-Regular.ttf 6");
		term_force_font(7, "Hack-Regular.ttf 6");
		break;
	case 1:
		/* change main window font */
		term_force_font(0, "Hack-Regular.ttf 12");
		/* Change sub windows too */
		term_force_font(1, "Hack-Regular.ttf 12");
		term_force_font(2, "Hack-Regular.ttf 12");
		term_force_font(3, "Hack-Regular.ttf 8");
		term_force_font(4, "Hack-Regular.ttf 10");
		term_force_font(5, "Hack-Regular.ttf 10");
		term_force_font(6, "Hack-Regular.ttf 8");
		term_force_font(7, "Hack-Regular.ttf 8");
		break;
	case 2:
		/* change main window font */
		term_force_font(0, "Hack-Regular.ttf 14");
		/* Change sub windows too */
		term_force_font(1, "Hack-Regular.ttf 14");
		term_force_font(2, "Hack-Regular.ttf 14");
		term_force_font(3, "Hack-Regular.ttf 10");
		term_force_font(4, "Hack-Regular.ttf 12");
		term_force_font(5, "Hack-Regular.ttf 12");
		term_force_font(6, "Hack-Regular.ttf 10");
		term_force_font(7, "Hack-Regular.ttf 10");
		break;
	case 3:
		/* change main window font */
		term_force_font(0, "Hack-Regular.ttf 16");
		/* Change sub windows too */
		term_force_font(1, "Hack-Regular.ttf 16");
		term_force_font(2, "Hack-Regular.ttf 16");
		term_force_font(3, "Hack-Regular.ttf 12");
		term_force_font(4, "Hack-Regular.ttf 14");
		term_force_font(5, "Hack-Regular.ttf 14");
		term_force_font(6, "Hack-Regular.ttf 12");
		term_force_font(7, "Hack-Regular.ttf 12");
		break;
	}
}

static void term_force_font(int term_idx, cptr fnt_name) {
	term_data *td = term_idx_to_term_data(term_idx);

	/* non-visible window has no fnt-> .. */
	if (!term_get_visibility(term_idx)) return;

	/* special hack: this window was invisible, but we just toggled
	   it to become visible on next client start. - C. Blue */
	if (!td->fnt) return;

	// Save current font size.
	int old_fnt_wid = td->fnt->wid;
	int old_fnt_hgt = td->fnt->hgt;

	/* Create and initialize font. */
	infofnt *new_font;
	MAKE(new_font, infofnt);
	infofnt *old_infofnt = Infofnt;
	Infofnt_set(new_font);
	if (Infofnt_init(fnt_name)) {
		/* Failed to initialize. */
		fprintf(stderr, "Error forcing the \"%s\" font on terminal %d\n", fnt_name, term_idx);
		Infofnt_set(old_infofnt);
		if(in_game) {
			plog_fmt("Failed to load the \"%s\" font!", fnt_name);
		}
		FREE(new_font, infofnt);
		return;
	} 

	/* New font was successfully initialized, free the old one and use the new one. */
	if (td->fnt->name) string_free(td->fnt->name);
	if (td->fnt->font) {
		if (td->fnt->type == FONT_TYPE_TTF) TTF_CloseFont((TTF_Font*)td->fnt->font);
		if (td->fnt->type == FONT_TYPE_PCF) PCF_CloseFont((PCF_Font*)td->fnt->font);
	}
	FREE(td->fnt, infofnt);
	td->fnt = new_font;

	/* Resize the windows if any "change" is needed */
	if ((old_fnt_wid != td->fnt->wid) || (old_fnt_hgt != td->fnt->hgt)) {

		resize_window_sdl2(term_idx, td->t.wid, td->t.hgt);

#ifdef USE_GRAPHICS
		/* Resize graphic tiles if needed too. */
		if (use_graphics) {
			// Recreate new tiles layers and initialize surfaces.
			fprintf(stderr, "jezek - reinitialize graphics after font changed for term id: %d\n", term_idx);
			term_data_init_graphics(td);

			if (td->tiles_layers == NULL || td->tilePreparation == NULL) {
				quit_fmt("Couldn't prepare images after font resize in terminal %d\n", term_idx);
			}
		}
#endif
	}
	SDL_UpdateWindowSurface(td->win->window);

	/* Reload custom font prefs on main screen font change */
	if (td == &term_main) handle_process_font_file();
}

/* Used for checking window position on mapping and saving window positions on quitting.
   Returns ret_x, ret_y containing window coordinates relative to display window. */
void terminal_window_real_coords_sdl2(int term_idx, int *ret_x, int *ret_y) {
	term_data *td = term_idx_to_term_data(term_idx);

	/* non-visible window has no window info .. */
	if (!term_get_visibility(term_idx)) {
		*ret_x = *ret_y = 0;
		return;
	}

	/* Get the window position relative to the display */
	SDL_GetWindowPosition(td->win->window, ret_x, ret_y);
}

/* Resizes the graphics terminal window described by 'td' to dimensions given in 'cols', 'rows' inputs.
 * Stops the resize timer and validates input.
 * Resizes the SDL2 window if current dimensions don't match the validated dimensions.
 * The terminal stored in 'td->t' is resized to desired size if needed.
 * When the window is the main window, update the screen globals, handle bigmap and notify server if in game.
 */
void resize_window_sdl2(int term_idx, int cols, int rows) {
	fprintf(stderr, "jezek - resize_window_sdl2(term_idx: %d, cols: %d, rows: %d)\n", term_idx, cols, rows);
	// The 'term_idx_to_term_data()' returns '&term_main' if 'term_idx' is out of bounds and it is not desired to resize term_main terminal window in that case, so validate before.
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;
	term_data *td = term_idx_to_term_data(term_idx);

	/* Clear timer. */
	if (td->resize_timer > 0) {
		td->resize_timer = 0;
	}

	/* Validate input dimensions. */
	/* Our 'term_data' indexes in 'term_idx' are the same as 'ang_term' indexes so it's safe to use 'validate_term_dimensions()'. */
	bool rounding_down = validate_term_dimensions(term_idx, &cols, &rows);
	/* Are we actually enlarging the window? */
	if (td == &term_main && rounding_down && screen_hgt == SCREEN_HGT) rows = MAX_SCREEN_HGT + SCREEN_PAD_Y;

	/* Calculate dimensions in pixels. */
	int wid_draw = cols * td->fnt->wid;
	int hgt_draw = rows * td->fnt->hgt;
	int wid_border = wid_draw + (2 * DEFAULT_SDL2_BORDER_WIDTH);
	int hgt_border = hgt_draw + (2 * DEFAULT_SDL2_BORDER_WIDTH);
	fprintf(stderr, "jezek -  resize_window_sdl2: wid_border: %d, hgt_border: %d\n", wid_border, hgt_border);

	// Save current Infowin and activated Term.
	infowin *iwin = Infowin;
	term *t = Term;

	// Activate Infowin and Term from term data belonging to term_idx..
	Infowin_set(td->win);
	Term_activate(&td->t);

	// Correct the size of the window if new calculated dimensions differ.
	if (Infowin->wb != wid_border || Infowin->hb != hgt_border) {
		// If window is maximized and the dimensions are not as calculated, after we try to correct the size, window manager will again maximize the window again. That will result in a resize loop. To disrupt the loop, don't correct the size of window, if window is maximized.
		uint32_t windowFlags = SDL_GetWindowFlags(td->win->window);
		if ((windowFlags & SDL_WINDOW_MAXIMIZED) == false) {
			fprintf(stderr, "jezek -  resize_window_sdl2: Correcting size of window\n");
			Infowin_resize(wid_border, hgt_border);
		}
	}

	// Calculate drawing box ad border dimensions.
	Infowin->wd = wid_draw;
	Infowin->hd = hgt_draw;
	Infowin->bw = (Infowin->wb - Infowin->wd + 1)/2;
	Infowin->bh = (Infowin->hb - Infowin->hd + 1)/2;

	//TODO jezek - If this is screen, copy the previous surface to new.
	// Window was resized, surface needs to be regenerated too.
	SDL_FreeSurface(Infowin->surface);
	Infowin->surface = SDL_GetWindowSurface(Infowin->window);

	// Make the changes go live (triggers on next c_message_add() call). No need to check if dimensions differ, Term_resize handles it.
	Term_resize(cols, rows);

	if (!in_game) {
		Infowin_wipe();
		Term_win_update(1, 0, 0);
		Term_redraw();
	}

	// Restore saved Infowin and Tterm.
	Infowin_set(iwin);
	Term_activate(t);

	// Main screen is special. Update the screen_wid/hgt globals if needed and notify server about it if in game.
	if (td == &term_main) {

		int new_screen_cols = cols - SCREEN_PAD_X;
		int new_screen_rows = rows - SCREEN_PAD_Y;

		// If in game, avoid bottom line of garbage left from big_screen when shrinking to normal screen.
		if (in_game && new_screen_rows < screen_hgt) clear_from(SCREEN_HGT + SCREEN_PAD_Y - 1);

		if (screen_wid != new_screen_cols || screen_hgt != new_screen_rows) {
			//TODO jezek - Test if following if statement can be deleted.
			// Actually since we already do proper calcs for 'rows = ' above with rounding_down and also in validate_term_main_dimensions(), this whole if-block here is 100% redundant! but w/e.
			// Allow only 22 and 44 map lines (normal vs big_map).
			if (new_screen_rows <= SCREEN_HGT) {
				new_screen_rows = SCREEN_HGT;
			} else if (new_screen_rows < MAX_SCREEN_HGT) {
				// Can currently not happen, because validate_term_main_dimensions() will cut it down; we use rounding_down as workaround!
				// Are we enlarging or shrinking the window?
				if (screen_hgt < new_screen_rows) {
					new_screen_rows = MAX_SCREEN_HGT;
				} else {
					new_screen_rows = SCREEN_HGT;
				}
			} else {
				new_screen_rows = MAX_SCREEN_HGT;
			}

			screen_wid = new_screen_cols;
			screen_hgt = new_screen_rows;

			if (in_game) {
				// Switch big_map mode.
#ifndef GLOBAL_BIG_MAP
				if (Client_setup.options[CO_BIGMAP] && rows == DEFAULT_TERM_HGT) {
					c_cfg.big_map = FALSE;
					Client_setup.options[CO_BIGMAP] = FALSE;
				} else if (!Client_setup.options[CO_BIGMAP] && rows != DEFAULT_TERM_HGT) {
					c_cfg.big_map = TRUE;
					Client_setup.options[CO_BIGMAP] = TRUE;
				}
#else
				if (global_c_cfg_big_map && rows == DEFAULT_TERM_HGT) {
					global_c_cfg_big_map = FALSE;
				} else if (!global_c_cfg_big_map && rows != DEFAULT_TERM_HGT) {
					global_c_cfg_big_map = TRUE;
				}
#endif
				// Notify server and ask for a redraw.
				Send_screen_dimensions();
			}
		}
	}

	// Ask server for a redraw if in game.
	if (in_game) cmd_redraw();
}

/* Resizes main terminal window to dimensions in input. */
/* Used for OS-specific resize_main_window() hook. */
void resize_main_window_sdl2(int cols, int rows) {
	resize_window_sdl2(0, cols, rows);
}

bool ask_for_bigmap(void) {
	return(ask_for_bigmap_generic());
}

const char* get_font_name(int term_idx) {
	term_data *td = term_idx_to_term_data(term_idx);

	if (td->fnt) return(td->fnt->name);
	if (strlen(term_prefs[term_idx].font)) return(term_prefs[term_idx].font);
	return(sdl2_terms_font_default[term_idx]);
}

void set_font_name(int term_idx, char* fnt) {
	fprintf(stderr, "jezek - set_font_name(term_idx: %d, fnt: %s)\n", term_idx, fnt);
	term_data *td;
	char valid_fnt[256];

	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) {
		fprintf(stderr, "Terminal index %d is out of bounds for set_font_name\n", term_idx);
		return;
	}

	strncpy(valid_fnt, fnt, sizeof(valid_fnt));
	valid_fnt[sizeof(valid_fnt)-1] = '\0';
	validate_font_format(valid_fnt, term_idx);

	if (!term_get_visibility(term_idx)) {
		/* Terminal is not visible, Do nothing, just change the font name in preferences. */
		if (strcmp(term_prefs[term_idx].font, valid_fnt) != 0) {
			strncpy(term_prefs[term_idx].font, valid_fnt, sizeof(term_prefs[term_idx].font));
			term_prefs[term_idx].font[sizeof(term_prefs[term_idx].font) - 1] = '\0';
		}
		return;
	}

	term_force_font(term_idx, valid_fnt);

	/* Redraw the terminal for which the font was forced. */
	td = term_idx_to_term_data(term_idx);
	if (&td->t != Term) {
		/* Terminal for which the font was forced is not activated. Activate, redraw and activate the terminal before. */
		term *old_term = Term;
		Term_activate(&td->t);
		Term_redraw();
		Term_activate(old_term);
	} else {
		/* Terminal for which the font was forced is currently activated. Just redraw. */
		Term_redraw();
	}
}

void term_toggle_visibility(int term_idx) {
	if (term_idx == 0) {
		fprintf(stderr, "Warning: Toggling visibility for main terminal window is not allowed\n");
		return;
	}

	if (term_get_visibility(term_idx)) {
		/* Window is visible. Save it, close it and free its resources. */

		/* Save window position, dimension and font to term_prefs, cause at quitting the nuke_hook won't be called for closed windows. */
		term_data_to_term_prefs(term_idx);
		term_prefs[term_idx].visible = false;

		/* Destroy window and free resources. */
		term_nuke(ang_term[term_idx]);
		ang_term[term_idx] = NULL;
		return;
	}
	/* Window is not visible. Create it and draw content. */

	/* Create and initialize terminal window. */
	errr err = sdl2_term_init(term_idx);
	/* After initializing the new window is active. Switch to main window. */
	Term_activate(&term_main.t);

	if (err) {
		fprintf(stderr, "Error initializing toggled X11 terminal window with index %d\n", term_idx);
		return;
	}
	/* Window was successfully created. */
	term_prefs[term_idx].visible = true;

	/* Mark all windows for content refresh. */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER | PW_MSGNOCHAT | PW_MESSAGE | PW_CHAT | PW_CLONEMAP | PW_SUBINVEN);//PW_LAGOMETER is called automatically, no need.
	fprintf(stderr, "jezek - term_toggle_visibility PW_MESSAGE\n");
}

/* Returns true if terminal window specified by term_idx is currently visible. */
bool term_get_visibility(int term_idx) {
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return(false);

	/* Only windows initialized in ang_term array are currently visible. */
	return((bool)ang_term[term_idx]);
}

void apply_window_decorations(void) {
	SDL_bool bordered = window_decorations ? SDL_TRUE : SDL_FALSE;
	for (int i = 0; i < ANGBAND_TERM_MAX; i++) {
		if (!term_get_visibility(i)) continue;
		term_data *td = term_idx_to_term_data(i);
		SDL_SetWindowBordered(td->win->window, bordered);
	}
}

void get_term_main_font_name(char *buf) {
	/* fonts aren't available in command-line mode */
	if (!strcmp(ANGBAND_SYS, "gcu")) {
		buf[0] = 0;
		return;
	}

	if (term_main.fnt->name) strcpy(buf, term_main.fnt->name);
	else strcpy(buf, "");
}

/* automatically store name+password to ini file if we're a new player? */
void store_crecedentials(void) {
	write_mangrc(TRUE, TRUE, FALSE);
}

/* Palette animation - 2018 *testing* */
void animate_palette(void) {
	//TODO jezek - Invalidate cache when changing palette colors?
	byte i;
	byte rv, gv, bv;
	unsigned long code;
	char cn[8], tmp[3];

	static bool init = FALSE;
	static unsigned char ac = 0x00; //animatio
	term_data *old_td;


	/* Initialise the palette once. For some reason colour_table[] is all zero'ed again at the beginning. */
	tmp[2] = 0;
	if (!init) {
		for (i = 0; i < BASE_PALETTE_SIZE; i++) {
			/* Extract desired values */
			rv = color_table[i][1];
			gv = color_table[i][2];
			bv = color_table[i][3];

			/* Extract a full color code */
			code = (rv << 16) | (gv << 8) | bv;
			sprintf(cn, "#%06lx", code);

			c_message_add(format("currently: [%d] %s -> %d (%d,%d,%d)", i, color_name[i], cn, rv, gv, bv));

			/* Save the "complex" codes */
			tmp[0] = color_name[i][1];
			tmp[1] = color_name[i][2];
			rv = strtol(tmp, NULL, 16);
			color_table[i][1] = rv;
			tmp[0] = color_name[i][3];
			tmp[1] = color_name[i][4];
			gv = strtol(tmp, NULL, 16);
			color_table[i][2] = gv;
			tmp[0] = color_name[i][5];
			tmp[1] = color_name[i][6];
			bv = strtol(tmp, NULL, 16);
			color_table[i][3] = bv;

			c_message_add(format("init to: %s = %d,%d,%d", color_name[i], rv, gv, bv));

			/* Save the "simple" code */
			//color_table[i][0] = win_pal[i];
			color_table[i][0] = '#';
		}
		init = TRUE;
		return;
	}


	/* Animate! */
	ac = (ac + 0x10) % 0x100;

	color_table[1][1] = 0;
	color_table[1][2] = 0xFF - ac;
	color_table[1][3] = 0xFF - ac;
	color_table[9][1] = ac;
	color_table[9][2] = 0;
	color_table[9][3] = 0;


	/* Save the default colors */
	for (i = 0; i < BASE_PALETTE_SIZE; i++) {
		/* Extract desired values */
		rv = color_table[i][1];
		gv = color_table[i][2];
		bv = color_table[i][3];

		/* Extract a full color code */
		//code = PALETTERGB(rv, gv, bv);
		code = (rv << 16) | (gv << 8) | bv;
		sprintf(cn, "#%06lx", code);

		/* Activate changes */
		if (strcmp(color_name[i], cn)) {
			/* Apply the desired color */
			sprintf(color_name[i], "#%06lx", code & 0xFFFFFFL);
			c_message_add(format("changed [%d] %d -> %d (%d,%d,%d)", i, color_name[i], code, rv, gv, bv));
		}
	}

	/* Activate the palette */
	for (i = 0; i < BASE_PALETTE_SIZE; ++i) {
		cptr cname = color_name[0];

		//XFreeGC(Metadpy->dpy, clr[i]->gc);
		//MAKE(clr[i], infoclr);
		Infoclr_set (clr[i]);
#if 0 /* no colors on this display? */
		if (Metadpy->color) cname = color_name[i];
		else if (i) cname = color_name[1];
#else
		cname = color_name[i];
#endif
		Infoclr_init_parse(cname, "bg", false);
	}

	old_td = (term_data*)(Term->data);
	/* Refresh aka redraw windows with new colour */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {

		if (!term_get_visibility(i)) continue;
		if (term_prefs[i].x == -32000 || term_prefs[i].y == -32000) continue;

		Term_activate(&term_idx_to_term_data(i)->t);
		//Term_redraw(); --stripped down to just:
		Term_xtra(TERM_XTRA_FRESH, 0);
	}
	Term_activate(&old_td->t);
}

#define PALANIM_OPTIMIZED /* KEEP SAME AS SERVER! */
/* Accept a palette entry index (NOT a TERM_ colour) and sets its R/G/B values from 0..255. - C. Blue */
void set_palette(byte c, byte r, byte g, byte b) {
	unsigned long code;
	char cn[8];
	cptr cname = color_name[0];//, bcname = "bg"; <- todo, for cleaner code
	term_data *old_td = (term_data*)(Term->data);

#ifdef PALANIM_OPTIMIZED
	/* Check for refresh marker at the end of a palette data transmission */
	if (c == 127 || c == 128) {
 		
		/* Refresh aka redraw the main window with new colour */
		if (!term_get_visibility(0)) return;
		if (term_prefs[0].x == -32000 || term_prefs[0].y == -32000) return;
		Term_activate(&term_idx_to_term_data(0)->t);
		/* Invalidate cache to ensure redrawal doesn't get cancelled by tile-caching */
//WiP, not functional		if (screen_icky) Term_switch_fully(0);
		if (c_cfg.gfx_palanim_repaint || (c_cfg.gfx_hack_repaint && !gfx_palanim_repaint_hack))
			/* Alternative function for flicker-free redraw - C. Blue */
			//Term_repaint(SCREEN_PAD_LEFT, SCREEN_PAD_TOP, screen_wid, screen_hgt);
			/* Include the UI elements, which is required if we use ANIM_FULL_PALETTE_FLASH or ANIM_FULL_PALETTE_LIGHTNING  - C. Blue */
			Term_repaint(0, 0, CL_WINDOW_WID, CL_WINDOW_HGT);
		else {
			Term_redraw();
			gfx_palanim_repaint_hack = FALSE;
		}
//WiP, not functional		if (screen_icky) Term_switch_fully(0);
		Term_activate(&old_td->t);
		return;
	}
#else
	if (c == 127 || c == 128) return; //just discard refresh marker
#endif

	color_table[c][1] = r;
	color_table[c][2] = g;
	color_table[c][3] = b;

	/* Extract a full color code */
	code = (r << 16) | (g << 8) | b;
	sprintf(cn, "#%06lx", code);

	/* Activate changes */
#ifndef EXTENDED_BG_COLOURS
	if (strcmp(color_name[c], cn))
		/* Apply the desired color */
		strcpy(color_name[c], cn);
#else
	/* Testing // For extended-bg colors, for now just animate the background part */
	if (c >= CLIENT_PALETTE_SIZE) { /* TERMX_.. */
		if (strcmp(color_ext_name[c - CLIENT_PALETTE_SIZE][1], cn))
			/* Apply the desired color */
			strcpy(color_ext_name[c - CLIENT_PALETTE_SIZE][1], cn);
	} else {
		/* Normal colour: Just set the foreground part */
		if (strcmp(color_name[c], cn))
			/* Apply the desired color */
			strcpy(color_name[c], cn);
	}
#endif

	/* Activate the palette */
	Infoclr_set(clr[c]);

#ifndef EXTENDED_BG_COLOURS
	/* Foreground colour */
	cname = color_name[c];
#else
	/* For extended colors actually use background colour instead, as this interests us most atm */
	if (c >= CLIENT_PALETTE_SIZE) /* TERMX_.. */
		cname = color_ext_name[c - CLIENT_PALETTE_SIZE][1];
	/* Foreground colour */
	else cname = color_name[c];
#endif

#ifdef EXTENDED_BG_COLOURS
	/* Just for testing for now.. */
	if (c >= CLIENT_PALETTE_SIZE) { /* TERMX_.. */
		/* Actually animate the 'bg' colour instead of the 'fg' colour (testing purpose) */
		Infoclr_init_parse(color_ext_name[c - CLIENT_PALETTE_SIZE][0], cname, false);
	} else
#endif
	Infoclr_init_parse(cname, "bg", false);

#ifndef PALANIM_OPTIMIZED
	/* Refresh aka redraw the main window with new colour */
	if (!term_get_visibility(0)) return;
	if (term_prefs[0].x == -32000 || term_prefs[0].y == -32000) return;
	Term_activate(&term_idx_to_term_data(0)->t);
	Term_xtra(TERM_XTRA_FRESH, 0);
	Term_activate(&old_td->t);
#endif
}

/* Gets R/G/B values from 0..255 for a specific terminal palette entry (not for a TERM_ colour). */
void get_palette(byte c, byte *r, byte *g, byte *b) {
	*r = clr[c]->fg.r;
	*g = clr[c]->fg.g;
	*b = clr[c]->fg.b;
}

/* Redraw all term windows with current palette values. */
void refresh_palette(void) {
	int i;
	term_data *old_td = (term_data*)(Term->data);

	set_palette(128, 0, 0, 0);

	/* Refresh aka redraw windows with new colour (term 0 is already done in set_palette(128) line above) */
	for (i = 1; i < ANGBAND_TERM_MAX; i++) {
		if (!term_get_visibility(i)) continue;
		if (term_prefs[i].x == -32000 || term_prefs[i].y == -32000) continue;

		Term_activate(&term_idx_to_term_data(i)->t);
		Term_redraw();
		//Term_xtra(TERM_XTRA_FRESH, 0);
	}

	Term_activate(&old_td->t);
}

void set_window_title_sdl2(int term_idx, cptr title) {
	term_data *td;

	/* The 'term_idx_to_term_data()' returns '&term_main' if 'term_idx' is out of bounds and it is not desired to resize term_main terminal window in that case, so validate before. */
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;

	/* Trying to change title in this state causes a crash */
	if (!term_get_visibility(term_idx)) return;
	if (term_prefs[term_idx].x == -32000 || term_prefs[term_idx].y == -32000) return;

	td = term_idx_to_term_data(term_idx);

	/* Save current Infowin. */
	infowin *iwin = Infowin;

	Infowin_set(td->win);
	Infowin_set_name(ang_term_name[term_idx]);

	/* Restore saved Infowin. */
	Infowin_set(iwin);
}

errr sdl2_win_term_main_screenshot(cptr name) {
       if (!term_main.win || !term_main.win->surface) return 1;

       /* Ensure latest contents are displayed */
       SDL_UpdateWindowSurface(term_main.win->window);

       SDL_Surface *surface = term_main.win->surface;
       SDL_Surface *conv = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
       if (!conv) return 1;

       if (IMG_SavePNG(conv, name) != 0) {
               SDL_Log("IMG_SavePNG failed: %s", IMG_GetError());
               SDL_FreeSurface(conv);
               return 1;
       }

       SDL_FreeSurface(conv);
       return 0;
}

/* PCF definitions and handling functions. */

#define PCF_HEADER              0x70636601  // 'p' 'c' 'f' + 0x01 in LSBFirst

#define PCF_PROPERTIES          (1<<0)
#define PCF_ACCELERATORS        (1<<1)
#define PCF_METRICS             (1<<2)
#define PCF_BITMAPS             (1<<3)
#define PCF_INK_METRICS         (1<<4)
#define PCF_BDF_ENCODINGS       (1<<5)
#define PCF_SWIDTHS             (1<<6)
#define PCF_GLYPH_NAMES         (1<<7)
#define PCF_BDF_ACCELERATORS    (1<<8)

#define PCF_DEFAULT_FORMAT      0x00000000
#define PCF_INKBOUNDS           0x00000200
#define PCF_ACCEL_W_INKBOUNDS   0x00000100
#define PCF_COMPRESSED_METRICS  0x00000100

#define PCF_GLYPH_PAD_MASK      (3<<0)            /* See the bitmap table for explanation */
#define PCF_BYTE_MASK           (1<<2)            /* If set then Most Sig Byte First */
#define PCF_BIT_MASK            (1<<3)            /* If set then Most Sig Bit First */
#define PCF_SCAN_UNIT_MASK      (3<<4)            /* See the bitmap table for explanation */

#define PCF_LSBFirst            0
#define PCF_MSBFirst            1
#define PCF_BYTE_ORDER(f)       (((f) & PCF_BYTE_MASK)?PCF_MSBFirst:PCF_LSBFirst)
#define PCF_GLYPHPADOPTIONS     4
#define PCF_GLYPH_PAD_INDEX(f)  ((f) & PCF_GLYPH_PAD_MASK)

#define PCF_FORMAT_MASK         0xFFFFFF00
#define PCF_FORMAT_MATCH(a,b)   (((a)&PCF_FORMAT_MASK) == ((b)&PCF_FORMAT_MASK))

typedef struct PCFTable PCFTable;
struct PCFTable {
	uint32_t type;
	uint32_t format;
	uint32_t size;
	uint32_t offset;
};

static uint32_t pcf_readU32LE(FILE *f) {
	uint8_t buf[4];
	if (fread(buf,1,4,f)!=4) return 0;
	return (uint32_t)(buf[0]) | ((uint32_t)(buf[1])<<8) | ((uint32_t)(buf[2])<<16) | ((uint32_t)(buf[3])<<24);
}

static uint16_t pcf_readU16LE(FILE *f) {
	uint8_t buf[2];
	if (fread(buf,1,2,f)!=2) return 0;
	return (uint16_t)(buf[0]) | ((uint16_t)(buf[1])<<8);
}

static uint8_t pcf_readU8(FILE *f) {
	uint8_t val;
	if (fread(&val,1,1,f)!=1) return 0;
	return val;
}

static uint32_t pcf_readU32BE(FILE *f) {
	uint8_t buf[4];
	if (fread(buf,1,4,f)!=4) return 0;
	return ((uint32_t)(buf[0])<<24) | ((uint32_t)(buf[1])<<16) |
		((uint32_t)(buf[2])<<8) |  (uint32_t)(buf[3]);
}

static uint16_t pcf_readU16BE(FILE *f) {
	uint8_t buf[2];
	if (fread(buf,1,2,f)!=2) return 0;
	return (uint16_t)((buf[0]<<8) | buf[1]);
}

static uint32_t pcf_readU32(FILE *f, uint32_t format) {
	if (PCF_BYTE_ORDER(format) == PCF_MSBFirst) return pcf_readU32BE(f); // Big endian.
	return pcf_readU32LE(f); // Little endian.
}

static uint16_t pcf_readU16(FILE *f, uint32_t format) {
	if (PCF_BYTE_ORDER(format) == PCF_MSBFirst) return pcf_readU16BE(f);
	return pcf_readU16LE(f);
}

static int16_t pcf_readS16(FILE *f, uint32_t format) {
	return (int16_t)pcf_readU16(f, format);
}

static uint32_t pcf_readLSB32(FILE *f) {
	return pcf_readU32LE(f);
}

// Load a monospace bitmap PCF font.
// The returned PCF_Font needs to be freed using PCF_CloseFont() function after finished using.
// Ref: https://fontforge.org/docs/techref/pcf-format.html
static PCF_Font* PCF_OpenFont(const char *name) {
	FILE *f;
	uint32_t header;

	PCFTable* tables;
	uint32_t ntables;

	PCFTable* metricsTab = NULL;
	uint32_t metricsFormat;
	bool metricsCompressed;
	int16_t charWidth, ascent, descent;

	PCFTable* encodingsTab = NULL;
	uint32_t encodingsFormat;
	int16_t min_char_or_byte2, max_char_or_byte2;
	int16_t min_byte1, max_byte1, default_char;
	int32_t nGlyphIndexes;

	PCFTable* bitmapTab = NULL;
	uint32_t bitmapFormat;
	uint32_t nbitmaps;
	uint32_t *glyphOffsets;
	uint32_t bitmapSizes[PCF_GLYPHPADOPTIONS];
	uint32_t bitmapSize;
	uint8_t *bitmapData;

	PCF_Font *font;
	uint32_t totalWidth;
	uint32_t totalHeight;
	bool bitmapLSBitFirst;
	uint32_t scanUnit;
	uint32_t rowPadding;
	uint32_t bytesPerLine;

	uint32_t i, g, x, y, bitIndex, b, color;
	void *row;

	// Open the file.
	f = fopen(name, "rb");
	if(!f) {
		fprintf(stderr, "Error: Unable to open file %s.\n", name);
		return NULL;
	}

	// Check PCF header.
	header = pcf_readLSB32(f);
	if (header != PCF_HEADER) {
		fclose(f);
		fprintf(stderr, "Error: Not a valid PCF file (header mismatch).\n");
		return NULL;
	}

	// PCF tables reading and checks.

	// Read number of tables stored in the file.
	ntables = pcf_readLSB32(f);
	if (ntables == 0) {
		fprintf(stderr, "Error: Invalid PCF file (ntables=0).\n");
		fclose(f);
		return NULL;
	}
	//fprintf(stderr, "jezek - LoadPCFasSurface: table ntables: %u\n", ntables);

	// Allocate memory for data on tables.
	C_MAKE(tables, ntables, PCFTable);
	if (!tables) {
		fprintf(stderr, "Error: Memory allocation failed.\n");
		fclose(f);
		return NULL;
	}

	// Read tables data until all wanted tables found.
	for (i = 0; i < ntables; i++) {
		tables[i].type   = pcf_readLSB32(f);
		tables[i].format = pcf_readLSB32(f);
		tables[i].size   = pcf_readLSB32(f);
		tables[i].offset = pcf_readLSB32(f);

		if (metricsTab == NULL && tables[i].type == PCF_METRICS) {
			metricsTab = &tables[i];
		}
		if (encodingsTab == NULL && tables[i].type == PCF_BDF_ENCODINGS) {
			encodingsTab = &tables[i];
		}
		if (bitmapTab == NULL && tables[i].type == PCF_BITMAPS) {
			bitmapTab = &tables[i];
		}
		if (metricsTab && encodingsTab && bitmapTab) {
			break;
		}
	}

	// Check if all necessary tables were found.
	if (!metricsTab) {
		fprintf(stderr, "Error: PCF_METRICS table not found.\n");
	}
	if (!encodingsTab) {
		fprintf(stderr, "Error: PCF_BDF_ENCODINGS table not found.\n");
	}
	if(!bitmapTab) {
		fprintf(stderr, "Error: PCF_BITMAPS table not found.\n");
	}
	if (!metricsTab || !encodingsTab || !bitmapTab) {
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Allocate memory for the font.
	MAKE(font, PCF_Font);

	// Read the font metrics and check if font is monospace.

	// Move the read head at the beginning of the metrics data.
	if (fseek(f, metricsTab->offset, SEEK_SET) != 0) {
		fprintf(stderr, "Error: fseek to met table failed.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read metrics format in LSB order and determine compression for metrics data.
	metricsFormat = pcf_readLSB32(f);
	metricsCompressed  = metricsFormat & PCF_COMPRESSED_METRICS;

	// Get the number of glyphs in this font.
	if (metricsCompressed) {
		font->nGlyphs = (uint32_t)pcf_readU16(f, metricsFormat);
	} else {
		font->nGlyphs = pcf_readU32(f, metricsFormat);
	}

	// Check for no glyphs.
	if (font->nGlyphs == 0) {
		fprintf(stderr, "Error: No glyphs found.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// For a monospace font, we take the first metric as the global dimension and check if the other glyphs are the same..
	for (i = 0; i < font->nGlyphs; i++ ) {
		if (metricsCompressed) {
			(void)pcf_readU8(f); // leftBearing
			(void)pcf_readU8(f); // rightBearing
			charWidth = (int16_t)(pcf_readU8(f) - 128); // charWidth
			ascent = (int16_t)(pcf_readU8(f) - 128); // ascent
			descent = (int16_t)(pcf_readU8(f) - 128); // descent
		} else {
			pcf_readS16(f, metricsFormat); // leftBearing
			pcf_readS16(f, metricsFormat); // rightBearing
			charWidth = pcf_readS16(f, metricsFormat); // charWidth
			ascent = pcf_readS16(f, metricsFormat); // ascent
			descent = pcf_readS16(f, metricsFormat); // descent
			pcf_readU16(f, metricsFormat); // character_attributes
		}

		if (i == 0) {
			font->glyphWidth = charWidth;
			font->glyphHeight = ascent + descent;
		} else {
			if (font->glyphWidth != charWidth || font->glyphHeight != (ascent + descent)) {
				fprintf(stderr, "Error: Font is not a monospace font.\n");
				PCF_CloseFont(font);
				FREE(tables, PCFTable);
				fclose(f);
				return NULL;
			}
		}
	}

	// Check for monospace font dimensions.
	if (font->glyphWidth == 0 || font->glyphHeight == 0) {
		fprintf(stderr, "Error: Invalid glyph metrics (width=%u, height=%u).\n", font->glyphWidth, font->glyphHeight);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read and check the encodings data.

	// Move to encodings data position start in the file.
	if (fseek(f, encodingsTab->offset, SEEK_SET) != 0) {
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		fprintf(stderr, "Error: fseek failed.\n");
		return NULL;
	}

	// Read and check the encodings format.
	encodingsFormat = pcf_readLSB32(f);
	if (!PCF_FORMAT_MATCH(encodingsFormat, PCF_DEFAULT_FORMAT)) {
		fprintf(stderr, "Error: Invalid encodings format (%x).\n", encodingsFormat);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read encoding parameters.
	min_char_or_byte2 = pcf_readS16(f, encodingsFormat);
	max_char_or_byte2 = pcf_readS16(f, encodingsFormat);
	min_byte1 = pcf_readS16(f, encodingsFormat);
	max_byte1 = pcf_readS16(f, encodingsFormat);
	default_char = pcf_readS16(f, encodingsFormat);

	if (min_byte1 != max_byte1 != 0) {
		//For single byte encodings min_byte1==max_byte1==0, and encoded values are between [min_char_or_byte2,max_char_or_byte2]. The glyph index corresponding to an encoding is glyphindex[encoding-min_char_or_byte2].
		fprintf(stderr, "Error: Only single byte encodings are supported.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	nGlyphIndexes = (max_char_or_byte2 - min_char_or_byte2 + 1) * (max_byte1 - min_byte1 + 1);

	// Allocate memory for all the glyph indexes
	C_MAKE(font->glyphIndexes, nGlyphIndexes, int16_t);
	if (!font->glyphIndexes) {
		fprintf(stderr, "Error: Memory allocation failed for glyphIndexes.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read the glyph indexes for all the encodings and use 'default_char' if there is no glyph index for encoding.
	for (i = 0; i < nGlyphIndexes; i++) {
		font->glyphIndexes[i] = pcf_readS16(f, encodingsFormat);
		if (font->glyphIndexes[i] == (int16_t)0xFFFF) font->glyphIndexes[i] = default_char;
	}

	// Read all the bitmap storage and pixel data an do all the validity checks.

	// Move to the bitmap data start position in the file.
	if (fseek(f, bitmapTab->offset, SEEK_SET) != 0) {
		fprintf(stderr, "Error: fseek failed.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read and check the bitmap format.
	bitmapFormat = pcf_readLSB32(f);
	if (!PCF_FORMAT_MATCH(bitmapFormat, PCF_DEFAULT_FORMAT)) {
		fprintf(stderr, "Error: Invalid bitmap format (%x).\n", bitmapFormat);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read the number of bitmaps stored in the data.
	nbitmaps = pcf_readU32(f, bitmapFormat);

	// Check if number of stored bitmaps corresponds with the number of glyphs.
	if (nbitmaps != font->nGlyphs) {
		fprintf(stderr, "Error: Number of bitmaps (%u) does not equal number of glyphs (%u).\n", nbitmaps, font->nGlyphs);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Allocate array for glyph offsets in the bitmap data.
	C_MAKE(glyphOffsets, nbitmaps, uint32_t);
	if (!glyphOffsets) {
		fprintf(stderr, "Error: Memory allocation failed for glyphOffsets.\n");
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read all the glyph offsets for bitmap into a array.
	for (i = 0; i < nbitmaps; i++) {
		glyphOffsets[i] = pcf_readU32(f, bitmapFormat);
	}

	// Read all the bitmap sizes.
	for (i = 0; i < PCF_GLYPHPADOPTIONS; i++) {
		bitmapSizes[i] = pcf_readU32(f, bitmapFormat);
	}

	// Determine the size of the bitmap pixel data.
	bitmapSize = bitmapSizes[PCF_GLYPH_PAD_INDEX(bitmapFormat)];

	// Allocate memory for bitmap pixel data.
	C_MAKE(bitmapData, bitmapSize, uint8_t); // This is only possible because sizeof(uint8_t) is 1.
	if (!bitmapData) {
		fprintf(stderr, "Error: Memory allocation failed for bitmap data.\n");
		C_FREE(glyphOffsets, nbitmaps, uint32_t);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// Read the bitmap pixel data.
	if (fread(bitmapData, 1, bitmapSize, f) != bitmapSize) {
		fprintf(stderr, "Error: Failed to read bitmap data.\n");
		C_FREE(bitmapData, bitmapSize, uint8_t);
		C_FREE(glyphOffsets, nbitmaps, uint32_t);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		fclose(f);
		return NULL;
	}

	// There is no need to read from file.
	fclose(f);

	// Write all the bitmap pixel data into an SDL surface. All the glyphs in one line.

	// Compute total width and height of the surface.
	totalWidth = font->glyphWidth * font->nGlyphs;
	totalHeight = font->glyphHeight;

	// Create the surface with desired dimensions.
	font->bitmap = SDL_CreateRGBSurfaceWithFormat(0, totalWidth, totalHeight, 32, SDL_PIXELFORMAT_RGBA32);
	if(!font->bitmap) {
		fprintf(stderr, "Error: SDL_CreateRGBSurfaceWithFormat failed.\n");
		C_FREE(bitmapData, bitmapSize, uint8_t);
		C_FREE(glyphOffsets, nbitmaps, uint32_t);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		return NULL;
	}

	// Compute all the parameters needed to convert bitmap pixel data into the surface.
	bitmapLSBitFirst = (bitmapFormat & PCF_BIT_MASK);
	scanUnit = 1 << ((bitmapFormat & PCF_SCAN_UNIT_MASK) >> 4);
	if (scanUnit != 1 && scanUnit != 2 && scanUnit != 4) {
		fprintf(stderr, "Error: Invalid scanUnit %d. Only 1, 2 and 4 are allowed.\n", scanUnit);
		C_FREE(bitmapData, bitmapSize, uint8_t);
		C_FREE(glyphOffsets, nbitmaps, uint32_t);
		PCF_CloseFont(font);
		FREE(tables, PCFTable);
		return NULL;
	}
	rowPadding = 1 << (bitmapFormat & PCF_GLYPH_PAD_MASK);
	bytesPerLine = (font->glyphWidth + 7) / 8;
	if ((bytesPerLine % rowPadding) != 0) {
		bytesPerLine = bytesPerLine + (rowPadding - (bytesPerLine % rowPadding));
	}

	// Render each glyph into the surface.
	for (g = 0; g < font->nGlyphs; g++) {
		for (y = 0; y < font->glyphHeight; y++) {
			// Compute the starting position of row "y" in glyph "g" in the bitmap pixel data.
			row = bitmapData + glyphOffsets[g] + y * bytesPerLine;
			for (x = 0; x < font->glyphWidth; x++) {
				// Get portion of data, where the pixel on position [x, y] for glyph "g" is encoded.
				switch (scanUnit) {
					case 1: b = ((uint8_t *)row)[x / 8]; break;
					case 2: b = ((uint16_t *)row)[x / 16]; break;
					case 4: b = ((uint32_t *)row)[x / 32]; break;
				}

				// Compute the bit index based on bit order for the pixel.
				bitIndex = x % 8;
				if (bitmapLSBitFirst) bitIndex = 7 - bitIndex;

				// Determine the pixel color.
				color = (b & (1 << bitIndex)) ? 0xFFFFFFFF : 0x00000000;

				// Write the pixel into the SDL surface.
				((uint32_t*)font->bitmap->pixels)[y * totalWidth + g * font->glyphWidth + x] = color;
			}
		}
	}

	// Free all the unneeded variables previously allocated.
	C_FREE(bitmapData, bitmapSize, uint8_t);
	C_FREE(glyphOffsets, nbitmaps, uint32_t);
	FREE(tables, PCFTable);

	// The bitmap contains letters in solid white ob black transparent background. Set blend mode and color key now, which will be useful in PCF_RenderText function.
	SDL_SetSurfaceBlendMode(font->bitmap, SDL_BLENDMODE_NONE);
	SDL_SetColorKey(font->bitmap, SDL_TRUE, SDL_MapRGBA(font->bitmap->format, 255, 255, 255, 0));

	return font;
}

// Free all the font resources from memory.
static void PCF_CloseFont(PCF_Font *font) {
	if (font == NULL) return;
	if (font->glyphIndexes != NULL) {
		free(font->glyphIndexes);
		font->glyphIndexes = NULL;
	}
	if (font->bitmap !=NULL) {
		SDL_FreeSurface(font->bitmap);
		font->bitmap = NULL;
	}
	font->nGlyphs = 0;
	font->glyphWidth = 0;
	font->glyphHeight = 0;
	FREE(font, PCF_Font);
}

// Returns a 32b RGBA surface, with text in 'str' rendered in one line using 'fg_color' as text color and 'bg_color' as background color.
// NULL will be returned, if some error occurs.
SDL_Surface* PCF_RenderText(struct PCF_Font* font, const char* str, SDL_Color fg_color, SDL_Color bg_color) {
	SDL_Surface *surface, *preparation;
	SDL_Rect src_rect, dest_rect;
	int text_length, width, height;
	int i, glyph_index;

	if (!font || !str || !font->bitmap || !font->glyphIndexes) {
		return NULL;
	}

	// Prepare needed variables.
	text_length = strlen(str);
	width = text_length * font->glyphWidth;
	height = font->glyphHeight;

	// Create surface which will be returned.
	surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
	if (!surface) {
		fprintf(stderr, "PCF_RenderText: Error creating resulting surface: %s\n", TTF_GetError());
		return NULL;
	}
	// Fill the returning surface with background color.
	SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, bg_color.r, bg_color.g, bg_color.b, bg_color.a));

	// If background and foreground colors are the same, there is no need for font rendering.
	if (fg_color.r == bg_color.r && fg_color.g == bg_color.g && fg_color.b == bg_color.b) {
			return surface;
	}

	// Create a surface for font preparation.
	preparation = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
	if (!preparation) {
		fprintf(stderr, "PCF_RenderText: Error creating preparation surface: %s\n", TTF_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}
	// Fill the preparation surface with foreground color.
	SDL_FillRect(preparation, NULL, SDL_MapRGBA(preparation->format, fg_color.r, fg_color.g, fg_color.b, fg_color.a));

	// Render font into the returning surface.
	src_rect = (SDL_Rect){0, 0, font->glyphWidth, font->glyphHeight};
	dest_rect = (SDL_Rect){0, 0, font->glyphWidth, font->glyphHeight};
	for (i = 0; i < text_length; ++i) {
		// Get index for the i-th glyph.
		glyph_index = font->glyphIndexes[(uint8_t)str[i]];
		if (glyph_index >= 0) {
			// Adjust the x component of source rectangle.
			src_rect.x = glyph_index * font->glyphWidth;

			// Render the glyph into the preparation surface.
			// The 'font->bitmap' has already SDL_BLENDMODE_NONE set with color key transparency at the end of PCF_OpenFont function. This will result in the glyph painted in 'fg_color' with transparent background.
			SDL_BlitSurface(font->bitmap, &src_rect, preparation, &dest_rect);
		}
		// Adjust the x component of the destination rectangle.
		dest_rect.x += font->glyphWidth;
	}

	// Blend the preparation onto resulting surface.
	SDL_BlitSurface(preparation, NULL, surface, NULL);

	SDL_FreeSurface(preparation);
	return surface;
}
#endif // USE_SDL2
