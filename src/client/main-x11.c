/* File: main-x11.c */

/* Purpose: One (awful) way to run Angband under X11	-BEN- */


#include "angband.h"


#ifdef USE_X11

/* #define USE_GRAPHICS */

#include "../common/z-util.h"
#include "../common/z-virt.h"
#include "../common/z-form.h"


#ifndef __MAKEDEPEND__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xatom.h>
#if 0
#include <X11/extensions/XTest.h> /* for turn_off_numlock() */
#endif
#endif /* __MAKEDEPEND__ */

#include <sys/types.h>
#ifdef REGEX_SEARCH
 #include <regex.h>
#endif
#include <time.h>

// gettimeofday() requires <sys/time.h>
#include <sys/time.h>

/*
 * Notes on Colors:
 *
 *   1) On a monochrome (or "fake-monochrome") display, all colors
 *   will be "cast" to "fg," except for the bg color, which is,
 *   obviously, cast to "bg".  Thus, one can ignore this setting.
 *
 *   2) Because of the inner functioning of the color allocation
 *   routines, colors may be specified as (a) a typical color name,
 *   (b) a hexidecimal color specification (preceded by a pound sign),
 *   or (c) by strings such as "fg", "bg", "zg".
 *
 *   3) Due to the workings of the init routines, many colors
 *   may also be dealt with by their actual pixel values.  Note that
 *   the pixel with all bits set is "zg = (1 << metadpy->depth) - 1", which
 *   is not necessarily either black or white.
 */


/**** Available Types ****/

/*
 * An X11 pixell specifier
 */
typedef unsigned long Pixell;

/*
 * The structures defined below
 */
typedef struct metadpy metadpy;
typedef struct infowin infowin;
typedef struct infoclr infoclr;
typedef struct infofnt infofnt;


/*
 * A structure summarizing a given Display.
 *
 *	- The Display itself
 *	- The default Screen for the display
 *	- The virtual root (usually just the root)
 *	- The default colormap (from a macro)
 *
 *	- The "name" of the display
 *
 *	- The socket to listen to for events
 *
 *	- The width of the display screen (from a macro)
 *	- The height of the display screen (from a macro)
 *	- The bit depth of the display screen (from a macro)
 *
 *	- The black Pixell (from a macro)
 *	- The white Pixell (from a macro)
 *
 *	- The background Pixell (default: black)
 *	- The foreground Pixell (default: white)
 *	- The maximal Pixell (Equals: ((2 ^ depth)-1), is usually ugly)
 *
 *	- Bit Flag: Force all colors to black and white (default: !color)
 *	- Bit Flag: Allow the use of color (default: depth > 1)
 *	- Bit Flag: We created 'dpy', and so should nuke it when done.
 */

struct metadpy {
	Display		*dpy;
	Screen		*screen;
	Window		root;
	Colormap	cmap;

	char		*name;

	int		fd;

	uint		width;
	uint		height;
	uint		depth;

	Pixell		black;
	Pixell		white;

	Pixell		bg;
	Pixell		fg;
	Pixell		zg;

	uint		mono:1;
	uint		color:1;
	uint		nuke:1;
};



/*
 * A Structure summarizing Window Information.
 *
 * I assume that a window is at most 30000 pixels on a side.
 * I assume that the root windw is also at most 30000 square.
 *
 *	- The Window
 *	- The current Input Event Mask
 *
 *	- The location of the window
 *	- The width, height of the window
 *	- The border width of this window
 *
 *	- Byte: 1st Extra byte
 *
 *	- Bit Flag: This window is currently Mapped
 *	- Bit Flag: This window needs to be redrawn
 *	- Bit Flag: This window has been resized
 *
 *	- Bit Flag: We should nuke 'win' when done with it
 *
 *	- Bit Flag: 1st extra flag
 *	- Bit Flag: 2nd extra flag
 *	- Bit Flag: 3rd extra flag
 *	- Bit Flag: 4th extra flag
 */

struct infowin {
	Window		win;
	long		mask;

	s16b		x, y;
	s16b		w, h;
	u16b		b;

	byte		byte1;

	uint		mapped:1;
	uint		redraw:1;
	uint		resize:1;

	uint		nuke:1;

	uint		flag1:1;
	uint		flag2:1;
	uint		flag3:1;
	uint		flag4:1;
};






/*
 * A Structure summarizing Operation+Color Information
 *
 *	- The actual GC corresponding to this info
 *
 *	- The Foreground Pixell Value
 *	- The Background Pixell Value
 *
 *	- Num (0-15): The operation code (As in Clear, Xor, etc)
 *	- Bit Flag: The GC is in stipple mode
 *	- Bit Flag: Destroy 'gc' at Nuke time.
 */

struct infoclr {
	GC		gc;

	Pixell		fg;
	Pixell		bg;

	uint		code:4;
	uint		stip:1;
	uint		nuke:1;
};



/*
 * A Structure to Hold Font Information
 *
 *	- The 'XFontStruct*' (yields the 'Font')
 *
 *	- The font name
 *
 *	- The default character width
 *	- The default character height
 *	- The default character ascent
 *
 *	- Byte: Pixel offset used during fake mono
 *
 *	- Flag: Force monospacing via 'wid'
 *	- Flag: Nuke info when done
 */

struct infofnt {
	XFontStruct		*info;

	cptr			name;

	s16b			wid;
	s16b			hgt;
	s16b			asc;

	byte			off;

	uint			mono:1;
	uint			nuke:1;
};


// (prototype)
static unsigned long create_pixel(Display *dpy, byte red, byte green, byte blue);




/* OPEN: x-metadpy.h */




/**** Available Macros ****/


/* Set current metadpy (Metadpy) to 'M' */
#define Metadpy_set(M) \
	Metadpy = M


/* Initialize 'M' using Display 'D' */
#define Metadpy_init_dpy(D) \
	Metadpy_init_2(D,cNULL)

/* Initialize 'M' using a Display named 'N' */
#define Metadpy_init_name(N) \
	Metadpy_init_2((Display*)(NULL),N)

/* Initialize 'M' using the standard Display */
#define Metadpy_init() \
	Metadpy_init_name("")


/* SHUT: x-metadpy.h */


/* OPEN: x-infowin.h */


/**** Available Macros ****/

/* Init an infowin by giving father as an (info_win*) (or NULL), and data */
#define Infowin_init_dad(D, X, Y, W, H, B, FG, BG) \
	Infowin_init_data(((D) ? ((D)->win) : (Window)(None)), \
	                  X, Y, W, H, B, FG, BG)


/* Init a top level infowin by pos,size,bord,Colors */
#define Infowin_init_top(X, Y, W, H, B, FG, BG) \
	Infowin_init_data(None, X, Y, W, H, B, FG, BG)


/* Request a new standard window by giving Dad infowin and X,Y,W,H */
#define Infowin_init_std(D, X, Y, W, H, B) \
	Infowin_init_dad(D, X, Y, W, H, B, Metadpy->bg,Metadpy->bg)


/* Set the current Infowin */
#define Infowin_set(I) \
	(Infowin = (I))



/* SHUT: x-infowin.h */


/* OPEN: x-infoclr.h */




/**** Available Macros  ****/

/* Set the current Infoclr */
#define Infoclr_set(C) \
	(Infoclr = (C))



/**** Available Macros (Requests) ****/

#define Infoclr_init_ppo(F, B, O, M) \
	Infoclr_init_data(F, B, O, M)

#define Infoclr_init_cco(F, B, O , M) \
	Infoclr_init_ppo(Infoclr_Pixell(F), Infoclr_Pixell(B), O, M)

#define Infoclr_init_ppn(F, B, O, M) \
	Infoclr_init_ppo(F, B, Infoclr_Opcode(O), M)

#define Infoclr_init_ccn(F, B, O, M) \
	Infoclr_init_cco(F, B, Infoclr_Opcode(O), M)


/* SHUT: x-infoclr.h */


/* OPEN: x-infofnt.h */



/**** Available Macros ****/

/* Set the current infofnt */
#define Infofnt_set(I) \
	(Infofnt = (I))


/* SHUT: x-infofnt.h */



/* OPEN: r-metadpy.h */

/* SHUT: r-metadpy.h */


/* OPEN: r-infowin.h */


/**** Available macros ****/

/* Errr: Expose Infowin */
#define Infowin_expose() \
	(!(Infowin->redraw = 1))

/* Errr: Unxpose Infowin */
#define Infowin_unexpose() \
	(Infowin->redraw = 0)


/* SHUT: r-infowin.h */


/* OPEN: r-infoclr.h */

/* SHUT: r-infoclr.h */


/* OPEN: r-infofnt.h */

/* SHUT: r-infofnt.h */




/* File: xtra-x11.c */


/*
 * The "default" values
 */
static metadpy metadpy_default;


/*
 * The "current" variables
 */
static metadpy *Metadpy = &metadpy_default;
static infowin *Infowin = (infowin*)(NULL);
static infoclr *Infoclr = (infoclr*)(NULL);
static infofnt *Infofnt = (infofnt*)(NULL);




long x11_win_root = 0, x11_win_term_main = 0;



/* OPEN: x-metadpy.c */


/*
 * Init the current metadpy, with various initialization stuff.
 *
 * Inputs:
 *	dpy:  The Display* to use (if NULL, create it)
 *	name: The name of the Display (if NULL, the current)
 *
 * Notes:
 *	If 'name' is NULL, but 'dpy' is set, extract name from dpy
 *	If 'dpy' is NULL, then Create the named Display
 *	If 'name' is NULL, and so is 'dpy', use current Display
 */
static errr Metadpy_init_2(Display *dpy, cptr name) {
	metadpy *m = Metadpy;

	/*** Open the display if needed ***/

	/* If no Display given, attempt to Create one */
	if (!dpy) {
		/* Attempt to open the display */
		dpy = XOpenDisplay(name);

		/* Failure */
		if (!dpy) {
			/* No name given, extract DISPLAY */
			if (!name) name = getenv("DISPLAY");

			/* No DISPLAY extracted, use default */
			if (!name) name = "(default)";

			/* Error */
			return(-1);
		}

		/* We WILL have to Nuke it when done */
		m->nuke = 1;
	}

	/* Since the Display was given, use it */
	else {
		/* We will NOT have to Nuke it when done */
		m->nuke = 0;
	}


	/*** Save some information ***/

	/* Save the Display itself */
	m->dpy = dpy;

	/* Get the Screen and Virtual Root Window */
	m->screen = DefaultScreenOfDisplay(dpy);
	m->root = RootWindowOfScreen(m->screen);

	x11_win_root = m->root;

	/* Get the default colormap */
	m->cmap = DefaultColormapOfScreen(m->screen);

	/* Extract the true name of the display */
	m->name = DisplayString(dpy);

	/* Extract the fd */
	m->fd = ConnectionNumber(Metadpy->dpy);

	/* Use the fd for select() - mikaelh */
	x11_socket = m->fd;

	/* Save the Size and Depth of the screen */
	m->width = WidthOfScreen(m->screen);
	m->height = HeightOfScreen(m->screen);
	m->depth = DefaultDepthOfScreen(m->screen);

	/* Save the Standard Colors */
	m->black = BlackPixelOfScreen(m->screen);
	m->white = WhitePixelOfScreen(m->screen);


	/*** Make some clever Guesses ***/

	/* Guess at the desired 'fg' and 'bg' Pixell's */
	m->bg = m->black;
	m->fg = m->white;

#ifdef CUSTOMIZE_COLOURS_FREELY
	/* Actually use fg-colour of index #0 (usually black) as the bg colour. */
	Pixell pixel = create_pixel(m->dpy, (client_color_map[0] & 0xFF0000) >> 16, (client_color_map[0] & 0x00FF00) >> 8, client_color_map[0] & 0x0000FF);
	m->bg = pixel;
#endif

	/* Calculate the Maximum allowed Pixel value.  */
	m->zg = (1 << m->depth) - 1;

	/* Save various default Flag Settings */
	m->color = ((m->depth > 1) ? 1 : 0);
	m->mono = ((m->color) ? 0 : 1);


	/*** All done ***/

	/* Return "success" ***/
	return(0);
}

/*
 * General Flush/ Sync/ Discard routine
 */
static errr Metadpy_update(int flush, int sync, int discard) {
	/* Flush if desired */
	if (flush) XFlush(Metadpy->dpy);

	/* Sync if desired, using 'discard' */
	if (sync) XSync(Metadpy->dpy, discard);

	/* Success */
	return(0);
}


/*
 * Make a simple beep
 */
static errr Metadpy_do_beep(void) {
	/* Make a simple beep */
	XBell(Metadpy->dpy, 100);

	return(0);
}


/* SHUT: x-metadpy.c */


/* OPEN: x-metadpy.c */


/*
 * Set the name (in the title bar) of Infowin
 */
static errr Infowin_set_name(cptr name) {
	Status st;
	XTextProperty tp;
	char buf[128];
	char *bp = buf;

	strcpy(buf, name);
	st = XStringListToTextProperty(&bp, 1, &tp);
	if (st) XSetWMName(Metadpy->dpy, Infowin->win, &tp);
	XFree(tp.value);

	return(0);
}

/*
 * Init an infowin by giving some data.
 *
 * Inputs:
 *	dad: The Window that should own this Window (if any)
 *	x,y: The position of this Window
 *	w,h: The size of this Window
 *	b:   The border width
 *	b_color, bg_color: Border and background colors
 *
 * Notes:
 *	If 'dad == None' assume 'dad == root'
 */
static errr Infowin_init_data(Window dad, int x, int y, int w, int h, unsigned int b, Pixell b_color, Pixell bg_color) {
	Window xid;

	/* Wipe it clean */
	WIPE(Infowin, infowin);

	/*** Error Check XXX ***/


	/*** Create the Window 'xid' from data ***/

	/* If no parent given, depend on root */
	if (dad == None) dad = Metadpy->root;

	/* Create the Window. */
	xid = XCreateSimpleWindow(Metadpy->dpy, dad, x, y, w, h, b, b_color, bg_color);
	if (xid == 0) {
		fprintf(stderr, "Error creating window on display: %s\n", Metadpy->name);
		return(1);
	}

	/* Start out selecting No events */
	XSelectInput(Metadpy->dpy, xid, 0L);

	/* Remember main window for making screenshots later via via 'import'. - C. Blue
	   Note that we assume that the first window initialized here is the main screen. */
	if (x11_win_root == dad && !x11_win_term_main) {
		/* We need this just for making screenshots later, so only set it differently
		   from FALSE if we don't actually have the required screenshot tool (ImageMagick's "import"): */
		if (!system("which import")) x11_win_term_main = xid;
	}

	/* Assign stuff */
	Infowin->win = xid;

	/* Apply the above info */
	Infowin->x = x;
	Infowin->y = y;
	Infowin->w = w;
	Infowin->h = h;
	Infowin->b = b;

	/* Apply the above info */
	Infowin->mask = 0L;
	Infowin->mapped = 0;

	/* And assume that we are exposed */
	Infowin->redraw = 1;

	/* Mark it as nukable */
	Infowin->nuke = 1;

	return(0);
}


/* SHUT: x-infowin.c */


/* OPEN: x-infoclr.c */


/*
 * A NULL terminated pair list of legal "operation names"
 *
 * Pairs of values, first is texttual name, second is the string
 * holding the decimal value that the operation corresponds to.
 */
static cptr opcode_pairs[] = {
	"cpy", "3",
	"xor", "6",
	"and", "1",
	"ior", "7",
	"nor", "8",
	"inv", "10",
	"clr", "0",
	"set", "15",

	"src", "3",
	"dst", "5",

	"dst & src", "1",
	"src & dst", "1",

	"dst | src", "7",
	"src | dst", "7",

	"dst ^ src", "6",
	"src ^ dst", "6",

	"+andReverse", "2",
	"+andInverted", "4",
	"+noop", "5",
	"+equiv", "9",
	"+orReverse", "11",
	"+copyInverted", "12",
	"+orInverted", "13",
	"+nand", "14",
	NULL
};


/*
 * Parse a word into an operation "code"
 *
 * Inputs:
 *	str: A string, hopefully representing an Operation
 *
 * Output:
 *	0-15: if 'str' is a valid Operation
 *	-1:   if 'str' could not be parsed
 */
static int Infoclr_Opcode(cptr str) {
	register int i;

	/* Scan through all legal operation names */
	for (i = 0; opcode_pairs[i * 2]; ++i) {
		/* Is this the right oprname? */
		if (streq(opcode_pairs[i * 2], str)) {
			/* Convert the second element in the pair into a Code */
			return(atoi(opcode_pairs[i * 2 + 1]));
		}
	}

	/* The code was not found, return -1 */
	return(-1);
}



/*
 * Request a Pixell by name.  Note: uses 'Metadpy'.
 *
 * Inputs:
 *      name: The name of the color to try to load (see below)
 *
 * Output:
 *	The Pixell value that metched the given name
 *	'Metadpy->fg' if the name was unparseable
 *
 * Valid forms for 'name':
 *	'fg', 'bg', 'zg', '<name>' and '#<code>'
 */
static Pixell Infoclr_Pixell(cptr name) {
	XColor scrn;


	/* Attempt to Parse the name */
	if (name && name[0]) {
		/* The 'bg' color is available */
		if (streq(name, "bg")) return(Metadpy->bg);

		/* The 'fg' color is available */
		if (streq(name, "fg")) return(Metadpy->fg);

		/* The 'zg' color is available */
		if (streq(name, "zg")) return(Metadpy->zg);

		/* The 'white' color is available */
		if (streq(name, "white")) return(Metadpy->white);

		/* The 'black' color is available */
		if (streq(name, "black")) return(Metadpy->black);

		/* Attempt to parse 'name' into 'scrn' */
		if (!(XParseColor(Metadpy->dpy, Metadpy->cmap, name, &scrn)))
			plog_fmt("Warning: Couldn't parse color '%s'\n", name);

		/* Attempt to Allocate the Parsed color */
		if (!(XAllocColor (Metadpy->dpy, Metadpy->cmap, &scrn)))
			plog_fmt("Warning: Couldn't allocate color '%s'\n", name);

		/* The Pixel was Allocated correctly */
		else return(scrn.pixel);
	}

	/* Warn about the Default being Used */
	plog_fmt("Warning: Using 'fg' for unknown color '%s'\n", name);

	/* Default to the 'Foreground' color */
	return(Metadpy->fg);
}

/*
 * Initialize an infoclr with some data
 *
 * Inputs:
 *	fg:   The Pixell for the requested Foreground (see above)
 *	bg:   The Pixell for the requested Background (see above)
 *	op:   The Opcode for the requested Operation (see above)
 *	stip: The stipple mode
 */
static errr Infoclr_init_data(Pixell fg, Pixell bg, int op, int stip) {
	infoclr *iclr = Infoclr;

	GC gc;
	XGCValues gcv;
	unsigned long gc_mask;



	/*** Simple error checking of opr and clr ***/

	/* Check the 'Pixells' for realism */
	if (bg > Metadpy->zg) return(-1);
	if (fg > Metadpy->zg) return(-1);

	/* Check the data for trueness */
	if ((op < 0) || (op > 15)) return(-1);


	/*** Create the requested 'GC' ***/

	/* Assign the proper GC function */
	gcv.function = op;

	/* Assign the proper GC background */
	gcv.background = bg;

	/* Assign the proper GC foreground */
	gcv.foreground = fg;

	/* Hack -- Handle XOR (xor is code 6) by hacking bg and fg */
	if (op == 6) gcv.background = 0;
	if (op == 6) gcv.foreground = (bg ^ fg);

	/* Assign the proper GC Fill Style */
	gcv.fill_style = (stip ? FillStippled : FillSolid);

	/* Turn off 'Give exposure events for pixmap copying' */
	gcv.graphics_exposures = False;

	/* Set up the GC mask */
	gc_mask = (GCFunction | GCBackground | GCForeground |
	           GCFillStyle | GCGraphicsExposures);

	/* Create the GC detailed above */
	gc = XCreateGC(Metadpy->dpy, Metadpy->root, gc_mask, &gcv);


	/*** Initialize ***/

	/* Wipe the iclr clean */
	WIPE(iclr, infoclr);

	/* Assign the GC */
	iclr->gc = gc;

	/* Nuke it when done */
	iclr->nuke = 1;

	/* Assign the parms */
	iclr->fg = fg;
	iclr->bg = bg;
	iclr->code = op;
	iclr->stip = stip ? 1 : 0;

	/* Success */
	return(0);
}

/*
 * Prepare a new 'infofnt'
 */
static errr Infofnt_prepare(XFontStruct *info) {
	infofnt *ifnt = Infofnt;

	XCharStruct *cs;

	/* Assign the struct */
	ifnt->info = info;

	/* Jump into the max bouonds thing */
	cs = &(info->max_bounds);

	/* Extract default sizing info */
	ifnt->asc = info->ascent;
	ifnt->hgt = info->ascent + info->descent;
	ifnt->wid = cs->width;

#ifdef OBSOLETE_SIZING_METHOD
	/* Extract default sizing info */
	ifnt->asc = cs->ascent;
	ifnt->hgt = (cs->ascent + cs->descent);
	ifnt->wid = cs->width;
#endif

	/* Success */
	return(0);
}

/*
 * Init an infofnt by its Name
 *
 * Inputs:
 *	name: The name of the requested Font
 */
static errr Infofnt_init_data(cptr name) {
	XFontStruct *info;


	/*** Load the info Fresh, using the name ***/

	/* If the name is not given, report an error */
	if (!name) return(-1);

	/* Attempt to load the font */
	info = XLoadQueryFont(Metadpy->dpy, name);

	/* The load failed, try to recover */
	if (!info) return(-1);


	/*** Init the font ***/

	/* Wipe the thing */
	WIPE(Infofnt, infofnt);

	/* Attempt to prepare it */
	if (Infofnt_prepare(info)) {
		/* Free the font */
		XFreeFont(Metadpy->dpy, info);

		/* Fail */
		return(-1);
	}

	/* Save a copy of the font name */
	Infofnt->name = string_make(name);

	/* Mark it as nukable */
	Infofnt->nuke = 1;

	/* Success */
	return(0);
}


/* SHUT: x-infofnt.c */



/* OPEN: r-metadpy.c */

/* SHUT: r-metadpy.c */

/* OPEN: r-infowin.c */

/*
 * Modify the event mask of an Infowin
 */
static errr Infowin_set_mask (long mask) {
	/* Save the new setting */
	Infowin->mask = mask;

	/* Execute the Mapping */
	XSelectInput(Metadpy->dpy, Infowin->win, Infowin->mask);

	/* Success */
	return(0);
}


/*
 * Request that Infowin be mapped
 */
static errr Infowin_map (void) {
	/* Execute the Mapping */
	XMapWindow(Metadpy->dpy, Infowin->win);

	/* Success */
	return(0);
}

/*
 * Request that Infowin be raised
 */
static errr Infowin_raise(void) {
	/* Raise towards visibility */
	XRaiseWindow(Metadpy->dpy, Infowin->win);

	/* Success */
	return(0);
}

/*
 * Request to focus Infowin.
 */
static errr Infowin_set_focus(void) {
	/* Set input focus for window. */
	XSetInputFocus(Metadpy->dpy, Infowin->win, RevertToNone, CurrentTime);

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
	XMoveWindow(Metadpy->dpy, Infowin->win, x, y);

	/* Success */
	return(0);
}

/*
 * Resize an infowin
 */
static errr Infowin_resize(int w, int h) {
	/* Execute the request */
	Infowin->w = w;
	Infowin->h = h;
	XResizeWindow(Metadpy->dpy, Infowin->win, w, h);

	/* Success */
	return(0);
}

/*
 * Visually clear Infowin
 */
static errr Infowin_wipe(void) {
	/* Execute the request */
	XClearWindow(Metadpy->dpy, Infowin->win);

	/* Success */
	return(0);
}

/*
 * Standard Text
 */
static errr Infofnt_text_std(int x, int y, cptr str, int len) {
	int i;


	/*** Do a brief info analysis ***/

	/* Do nothing if the string is null */
	if (!str || !*str) return(-1);

	/* Get the length of the string */
	if (len < 0) len = strlen (str);


	/*** Decide where to place the string, vertically ***/

	/* Ignore Vertical Justifications */
	y = (y * Infofnt->hgt) + Infofnt->asc;


	/*** Decide where to place the string, horizontally ***/

	/* Line up with x at left edge of column 'x' */
	x = (x * Infofnt->wid);


	/*** Actually draw 'str' onto the infowin ***/

	/* Be sure the correct font is ready */
	XSetFont(Metadpy->dpy, Infoclr->gc, Infofnt->info->fid);


	/*** Handle the fake mono we can enforce on fonts ***/

	/* Monotize the font */
	if (Infofnt->mono) {
		/* Do each character */
		for (i = 0; i < len; ++i)
			/* Note that the Infoclr is set up to contain the Infofnt */
			XDrawImageString(Metadpy->dpy, Infowin->win, Infoclr->gc,
			                 x + i * Infofnt->wid + Infofnt->off, y, str + i, 1);
	}

	/* Assume monospaced font */
	else
		/* Note that the Infoclr is set up to contain the Infofnt */
		XDrawImageString(Metadpy->dpy, Infowin->win, Infoclr->gc,
		                 x, y, str, len);

	/* Success */
	return(0);
}


/*
 * Painting where text would be
 */
static errr Infofnt_text_non(int x, int y, cptr str, int len) {
	int w, h;


	/*** Find the width ***/

	/* Negative length is a flag to count the characters in str */
	if (len < 0) len = strlen(str);

	/* The total width will be 'len' chars * standard width */
	w = len * Infofnt->wid;


	/*** Find the X dimensions ***/

	/* Line up with x at left edge of column 'x' */
	x = x * Infofnt->wid;


	/*** Find other dimensions ***/

	/* Simply do 'Infofnt->hgt' (a single row) high */
	h = Infofnt->hgt;

	/* Simply do "at top" in row 'y' */
	y = y * h;


	/*** Actually 'paint' the area ***/

	/* Just do a Fill Rectangle */
	XFillRectangle(Metadpy->dpy, Infowin->win, Infoclr->gc, x, y, w, h);

	/* Success */
	return(0);
}





/* SHUT: r-infofnt.c */




/* OPEN: main-x11.c */


#ifndef IsModifierKey

/*
 * Keysym macros, used on Keysyms to test for classes of symbols
 * These were stolen from one of the X11 header files
 */

#define IsKeypadKey(keysym) \
    (((unsigned)(keysym) >= XK_KP_Space) && ((unsigned)(keysym) <= XK_KP_Equal))

#define IsCursorKey(keysym) \
    (((unsigned)(keysym) >= XK_Home)     && ((unsigned)(keysym) <  XK_Select))

#define IsPFKey(keysym) \
    (((unsigned)(keysym) >= XK_KP_F1)     && ((unsigned)(keysym) <= XK_KP_F4))

#define IsFunctionKey(keysym) \
    (((unsigned)(keysym) >= XK_F1)       && ((unsigned)(keysym) <= XK_F35))

#define IsMiscFunctionKey(keysym) \
    (((unsigned)(keysym) >= XK_Select)   && ((unsigned)(keysym) <  XK_KP_Space))

#define IsModifierKey(keysym) \
    (((unsigned)(keysym) >= XK_Shift_L)  && ((unsigned)(keysym) <= XK_Hyper_R))

#endif


/*
 * Checks if the keysym is a special key or a normal key
 * Assume that XK_MISCELLANY keysyms are special
 */
#define IsSpecialKey(keysym) \
    ((unsigned)(keysym) >= 0xFF00)


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
void terminal_window_real_coords_x11(int term_idx, int *ret_x, int *ret_y);
void resize_main_window_x11(int cols, int rows);
void resize_window_x11(int term_idx, int cols, int rows);
static term_data* term_idx_to_term_data(int term_idx);


/* Cache for prepared graphical tiles.
   Observations:
    Size 256:    Cache fills maybe within first 10 floors of Barrow-Downs (2mask mode), so it's very comfortable.
                 However, it overflows instantly in just 1 sector of housing area around Bree, on admin who can see all objects.
    Size 256*3:  Cache manages to more or less capture a whole housing area sector fine. This seems a good minimum cache size.
    Size 256*4:  Default choice now, for reserves.
*/
#define TILE_CACHE_SIZE (256*4)

/* Output cache state information in the message window? Spammy and only for debugging purpose. */
//#define TILE_CACHE_LOG

#if 1 /* Enable extra cache efficiency regarding palette animation? */
 #if 1 /* Choose a cache behaviour aspect regarding palette animation, the 1st is more efficient */
 /* Cache invalidates based on attr when new palette info is received? -- more efficient */
  #define TILE_CACHE_NEWPAL
 #else
 /* Cache uses foreground + background colour palette values too? */
  #define TILE_CACHE_FGBG
 #endif
#endif

#ifdef TILE_CACHE_SIZE
extern bool disable_tile_cache;
bool disable_tile_cache = FALSE;
struct tile_cache_entry {
    Pixmap tilePreparation;
    char32_t c;
    byte a;
 #ifdef GRAPHICS_BG_MASK
    Pixmap tilePreparation2;
    char32_t c_back;
    byte a_back;
 #endif
    bool is_valid;
 #ifdef TILE_CACHE_FGBG
    s32b fg, bg; /* Optional palette_animation handling */
 #endif
};
#endif


/*
 * A structure for each "term"
 */
struct term_data {
	term t;

	infofnt *fnt;

	infowin *outer;
	infowin *inner;
	struct timeval resize_timer;

#ifdef USE_GRAPHICS

	XImage *tiles;
	Pixmap bgmask;
	Pixmap fgmask;
 #ifdef GRAPHICS_BG_MASK
	Pixmap bg2mask;
	Pixmap tilePreparation2;
 #endif
	Pixmap tilePreparation;

 #ifdef TILE_CACHE_SIZE
	int cache_position;
	struct tile_cache_entry tile_cache[TILE_CACHE_SIZE];
 #endif

#endif
};

#define DEFAULT_X11_OUTER_BORDER_WIDTH 1
#define DEFAULT_X11_INNER_BORDER_WIDTH 1

#define MICROSECONDS_IN_SECOND 1000000
static void timeval_add_us(struct timeval *t, unsigned int us) {
	t->tv_usec = t->tv_usec + us;
	while (t->tv_usec > MICROSECONDS_IN_SECOND) {
		t->tv_sec++;
		t->tv_usec -= MICROSECONDS_IN_SECOND;
	}
}

/* The main screen */
static term_data term_main;
/* The optional windows */
static term_data term_1;
static term_data term_2;
static term_data term_3;
static term_data term_4;
static term_data term_5;
static term_data term_6;
static term_data term_7;
static term_data term_8;
static term_data term_9;

static term_data *x11_terms_term_data[ANGBAND_TERM_MAX] = {&term_main, &term_1, &term_2, &term_3, &term_4, &term_5, &term_6, &term_7, &term_8, &term_9};
static char *x11_terms_font_env[ANGBAND_TERM_MAX] = {"TOMENET_X11_FONT_TERM_MAIN", "TOMENET_X11_FONT_TERM_1", "TOMENET_X11_FONT_TERM_2", "TOMENET_X11_FONT_TERM_3", "TOMENET_X11_FONT_TERM_4", "TOMENET_X11_FONT_TERM_5", "TOMENET_X11_FONT_TERM_6", "TOMENET_X11_FONT_TERM_7", "TOMENET_X11_FONT_TERM_8", "TOMENET_X11_FONT_TERM_9"};
static char *x11_terms_font_default[ANGBAND_TERM_MAX] = {DEFAULT_X11_FONT_TERM_MAIN, DEFAULT_X11_FONT_TERM_1, DEFAULT_X11_FONT_TERM_2, DEFAULT_X11_FONT_TERM_3, DEFAULT_X11_FONT_TERM_4, DEFAULT_X11_FONT_TERM_5, DEFAULT_X11_FONT_TERM_6, DEFAULT_X11_FONT_TERM_7, DEFAULT_X11_FONT_TERM_8, DEFAULT_X11_FONT_TERM_9};

/*
 * Set the size hints of Infowin
 */
static errr Infowin_set_size_hints(int x, int y, int w, int h, int b, int r_w, int r_h, bool fixed) {
	XSizeHints *sh;

	/* Make Size Hints */
	sh = XAllocSizeHints();

	/* Oops */
	if (!sh) return(1);

	/* Fixed window size */
	if (fixed) {
		sh->flags = PMinSize | PMaxSize;
		sh->min_width = sh->max_width = w;
		sh->min_height = sh->max_height = h;
	}

	/* Variable window size */
	else {
		sh->flags = PMinSize;
		sh->min_width = r_w + (2 * b);
		sh->min_height = r_h + (2 * b);
	}

	/* Standard fields */
	sh->width = w;
	sh->height = h;
	sh->width_inc = r_w;
	sh->height_inc = r_h;
	sh->base_width = 2 * b;
	sh->base_height = 2 * b;

	/* Useful settings */
	sh->flags |= PPosition | PSize | PResizeInc | PBaseSize;

	/* Use the size hints */
	XSetWMNormalHints(Metadpy->dpy, Infowin->win, sh);

	/* Success */
	XFree(sh);
	return(0);
}


/*
 * Set the name (in the title bar) of Infowin
 */
static errr Infowin_set_class_hint(cptr name) {
	XClassHint *ch;

	char res_name[20];
	char res_class[20];

	ch = XAllocClassHint();
	if (ch == NULL) return(1);

	strcpy(res_name, name);
	res_name[0] = FORCELOWER(res_name[0]);
	ch->res_name = res_name;

	strcpy(res_class, "TomeNET");
	ch->res_class = res_class;

	XSetClassHint(Metadpy->dpy, Infowin->win, ch);

	XFree(ch);
	return(0);
}



/*
 * Process a keypress event
 */
static void react_keypress(XEvent *xev) {
	int i, n, mc, ms, mo, mx;
	uint ks1;

	XKeyEvent *ev = (XKeyEvent*)(xev);
	KeySym ks;

	char buf[128];
	char msg[128];


	/* Check for "normal" keypresses */
	n = XLookupString(ev, buf, 125, &ks, NULL);

	/* Terminate */
	buf[n] = '\0';

	/* Hack -- convert into an unsigned int */
	ks1 = (uint)(ks);

	/* Extract four "modifier flags" */
	mc = (ev->state & ControlMask) ? TRUE : FALSE;
	ms = (ev->state & ShiftMask) ? TRUE : FALSE;
	mo = (ev->state & Mod1Mask) ? TRUE : FALSE;

	/* This is the NumLock state and usually it only causes problems - mikaelh */
	//mx = (ev->state & Mod2Mask) ? TRUE : FALSE;
	mx = FALSE;


	/* Hack -- Ignore "modifier keys" */
	if (IsModifierKey(ks)) return;

#ifdef ENABLE_SHIFT_SPECIALKEYS
	/* As we're shortcutting the whole key-evaluation with the various 'return' calls here, set the shiftkey flags manually */
	if (ms) inkey_shift_special |= 0x1;
	if (mc) inkey_shift_special |= 0x2;
	if (mo) inkey_shift_special |= 0x4;
#endif

	/* Normal keys with no modifiers */
	if (n && !mo && !mx && !IsSpecialKey(ks)) {
		/* Enqueue the normal key(s) */
		for (i = 0; buf[i]; i++) Term_keypress(buf[i]);

		/* All done */
		return;
	}

	/* Note, regarding new inkey_shift_special code:
	   We need this switch() code here, to get clean keys returned from get_macro_trigger(), when pressing ESC key! (No idea if there are more cases than this): */
	/* Handle a few standard keys */
	switch (ks1) {
		case XK_Escape:
		Term_keypress(ESCAPE); return;

		case XK_Return:
		Term_keypress('\r'); return;

		case XK_Tab:
		Term_keypress('\t'); return;

		case XK_Delete:
		case XK_BackSpace:
		Term_keypress('\010'); return;
	}

	/* Hack -- Use the KeySym */
	if (ks) {
		sprintf(msg, "%c%s%s%s%s_%lX%c", 31,
		        mc ? "N" : "", ms ? "S" : "",
		        mo ? "O" : "", mx ? "M" : "",
		        (unsigned long)(ks), 13);
	}
	/* Hack -- Use the Keycode */
	else {
		sprintf(msg, "%c%s%s%s%sK_%X%c", 31,
		        mc ? "N" : "", ms ? "S" : "",
		        mo ? "O" : "", mx ? "M" : "",
		        ev->keycode, 13);
	}

	/* Enqueue the "fake" string */
	for (i = 0; msg[i]; i++) Term_keypress(msg[i]);


	/* Hack -- dump an "extra" string */
	if (n) {
		/* Start the "extra" string */
		Term_keypress(28);

		/* Enqueue the "real" string */
		for (i = 0; buf[i]; i++) Term_keypress(buf[i]);

		/* End the "extra" string */
		Term_keypress(28);
	}
}


/*
 * Handles all terminal windows resize timers.
 */
static void resize_window_x11_timers_handle(void) {
	struct timeval now = {0, 0};
	int t_idx;

	for (t_idx = 0; t_idx < ANGBAND_TERM_MAX; t_idx++) {
		term_data *td = term_idx_to_term_data(t_idx);

		if (td->resize_timer.tv_usec != 0 || td->resize_timer.tv_sec != 0) {
			if (now.tv_usec == 0 && now.tv_sec == 0) gettimeofday(&now, NULL);
			if (now.tv_sec > td->resize_timer.tv_sec || (now.tv_sec == td->resize_timer.tv_sec && now.tv_usec > td->resize_timer.tv_usec)) {
				td->resize_timer.tv_sec=0;
				td->resize_timer.tv_usec=0;

				resize_window_x11(t_idx, (td->outer->w - (2 * td->inner->b)) / td->fnt->wid, (td->outer->h - (2 * td->inner->b)) / td->fnt->hgt);

				/* In case we resized Chat/Msg/Msg+Chat window,
				   refresh contents so they are displayed properly,
				   without having to wait for another incoming message to do it for us. */
				p_ptr->window |= PW_MESSAGE | PW_CHAT | PW_MSGNOCHAT;
			}
		}
	}
}


/*
 * Process events
 */
static errr CheckEvent(bool wait) {
	term_data *old_td = (term_data*)(Term->data);

	XEvent xev_body, *xev = &xev_body;

	term_data *td = NULL;
	infowin *iwin = NULL;

	//int flag = 0;
	//int x, y, data;

	int t_idx = -1;


	/* Handle all window resize timers. */
	resize_window_x11_timers_handle();

	/* Do not wait unless requested */
	if (!wait) {
		/* The XPending() call caused an X11 error once:

		X Error of failed request:  BadMatch (invalid parameter attributes)
		  Major opcode of failed request:  42 (X_SetInputFocus)
		  Serial number of failed request:  12
		  Current serial number in output stream:  12

		It was suggested that this can happen due to asynchronous vs synchronous clashes in bad X11 code and to rememdy this, XSync() can be added.
		I don't know if this really works, and the bug is hard to reproduce (timing issues with windows being drawable or not perhaps):

		Also see here:
		https://www.ultraengine.com/community/blogs/entry/2690-the-year-of-the-linux-desktop-is-here/
		quote:
		"The way around this is to call XMapWindow and then wait on the event queue until a MapNotify event for that window occurs,
		adding all other events into a list that can be evaluated on the next call to WaitEvent().
		The time elapsed is checked inside the loop in case something weird happens and the desired event is never received"

		*/
		XSync(Metadpy->dpy, 0);

		/* This caused the X11 error above in rare circumstances. Shouldn't matter during actual gameplay though, only if window states change. */
		if (!XPending(Metadpy->dpy)) return(1);
	}

	/* Load the Event */
	XNextEvent(Metadpy->dpy, xev);


	/* Notice new keymaps */
	if (xev->type == MappingNotify) {
		XRefreshKeyboardMapping(&xev->xmapping);
		return(0);
	}


	/* Main screen, inner window */
	if (xev->xany.window == term_main.inner->win) {
		td = &term_main;
		iwin = td->inner;
		t_idx = 0;
	}
	/* Main screen, outer window */
	else if (xev->xany.window == term_main.outer->win) {
		td = &term_main;
		iwin = td->outer;
		t_idx = 0;
	}

	/* Mirror window, inner window */
	else if (term_term_1 && xev->xany.window == term_1.inner->win) {
		td = &term_1;
		iwin = td->inner;
		t_idx = 1;
	}
	/* Mirror window, outer window */
	else if (term_term_1 && xev->xany.window == term_1.outer->win)
	{
		td = &term_1;
		iwin = td->outer;
		t_idx = 1;
	}

	/* Recall window, inner window */
	else if (term_term_2 && xev->xany.window == term_2.inner->win) {
		td = &term_2;
		iwin = td->inner;
		t_idx = 2;
	}
	/* Recall window, outer window */
	else if (term_term_2 && xev->xany.window == term_2.outer->win) {
		td = &term_2;
		iwin = td->outer;
		t_idx = 2;
	}

	/* Choice window, inner window */
	else if (term_term_3 && xev->xany.window == term_3.inner->win) {
		td = &term_3;
		iwin = td->inner;
		t_idx = 3;
	}
	/* Choice window, outer window */
	else if (term_term_3 && xev->xany.window == term_3.outer->win) {
		td = &term_3;
		iwin = td->outer;
		t_idx = 3;
	}

	/* Other window, inner window */
	else if (term_term_4 && xev->xany.window == term_4.inner->win) {
		td = &term_4;
		iwin = td->inner;
		t_idx = 4;
	}
	/* Other window, outer window */
	else if (term_term_4 && xev->xany.window == term_4.outer->win) {
		td = &term_4;
		iwin = td->outer;
		t_idx = 4;
	}

	/* Other window, inner window */
	else if (term_term_5 && xev->xany.window == term_5.inner->win) {
		td = &term_5;
		iwin = td->inner;
		t_idx = 5;
	}
	/* Other window, outer window */
	else if (term_term_5 && xev->xany.window == term_5.outer->win) {
		td = &term_5;
		iwin = td->outer;
		t_idx = 5;
	}

	/* Other window, inner window */
	else if (term_term_6 && xev->xany.window == term_6.inner->win) {
		td = &term_6;
		iwin = td->inner;
		t_idx = 6;
	}
	/* Other window, outer window */
	else if (term_term_6 && xev->xany.window == term_6.outer->win) {
		td = &term_6;
		iwin = td->outer;
		t_idx = 6;
	}

	/* Other window, inner window */
	else if (term_term_7 && xev->xany.window == term_7.inner->win) {
		td = &term_7;
		iwin = td->inner;
		t_idx = 7;
	}
	/* Other window, outer window */
	else if (term_term_7 && xev->xany.window == term_7.outer->win) {
		td = &term_7;
		iwin = td->outer;
		t_idx = 7;
	}

	/* Other window, inner window */
	else if (term_term_8 && xev->xany.window == term_8.inner->win) {
		td = &term_8;
		iwin = td->inner;
		t_idx = 8;
	}
	/* Other window, outer window */
	else if (term_term_8 && xev->xany.window == term_8.outer->win) {
		td = &term_8;
		iwin = td->outer;
		t_idx = 8;
	}

	/* Other window, inner window */
	else if (term_term_9 && xev->xany.window == term_9.inner->win) {
		td = &term_9;
		iwin = td->inner;
		t_idx = 9;
	}
	/* Other window, outer window */
	else if (term_term_9 && xev->xany.window == term_9.outer->win) {
		td = &term_9;
		iwin = td->outer;
		t_idx = 9;
	}


	/* Unknown window */
	if (!td || !iwin) return(0);


	/* Hack -- activate the Term */
	Term_activate(&td->t);

	/* Hack -- activate the window */
	Infowin_set(iwin);

	/* Switch on the Type */
	switch (xev->type) {
		/* A Button Press Event */
		case ButtonPress:
			/* Set flag, then fall through */
			//flag = 1;

		/* A Button Release (or ButtonPress) Event */
		case ButtonRelease:
			/* Which button is involved */
			/* if      (xev->xbutton.button == Button1) data = 1;
			else if (xev->xbutton.button == Button2) data = 2;
			else if (xev->xbutton.button == Button3) data = 3;
			else if (xev->xbutton.button == Button4) data = 4;
			else if (xev->xbutton.button == Button5) data = 5; */

			/* Where is the mouse */
			//x = xev->xbutton.x;
			//y = xev->xbutton.y;

			/* XXX Handle */

			break;

		/* An Enter Event */
		case EnterNotify:
			/* Note the Enter, Fall into 'Leave' */
			//flag = 1;

		/* A Leave (or Enter) Event */
		case LeaveNotify:
			/* Where is the mouse */
			//x = xev->xcrossing.x;
			//y = xev->xcrossing.y;

			/* XXX Handle */

			break;

		/* A Motion Event */
		case MotionNotify:
			/* Where is the mouse */
			//x = xev->xmotion.x;
			//y = xev->xmotion.y;

			/* XXX Handle */

			break;

		/* A KeyRelease */
		case KeyRelease:
			/* Nothing */
			break;

		/* A KeyPress */
		case KeyPress:
			/* Save the mouse location */
			//x = xev->xkey.x;
			//y = xev->xkey.y;

			/* Hack -- use "old" term */
			Term_activate(&old_td->t);

			/* Process the key */
			react_keypress(xev);

			break;

		/* An Expose Event */
		case Expose:
			/* Redraw (if allowed) */
			if (iwin == td->inner) {
				/* Get the area that should be updated */
				int x1 = xev->xexpose.x / td->fnt->wid;
				int y1 = xev->xexpose.y / td->fnt->hgt;
				int x2 = ((xev->xexpose.x + xev->xexpose.width) / td->fnt->wid);
				int y2 = ((xev->xexpose.y + xev->xexpose.height) / td->fnt->hgt);

				/* Redraw section*/
				Term_redraw_section(x1, y1, x2, y2);
			}
			/* Clear the window */
			else Infowin_wipe();

			break;

		/* A Mapping Event */
		case MapNotify:
			Infowin->mapped = 1;

			/* Hints were sent to X with window position from term_prefs at window creation, so it should have right position at mapping.
			 * But in case the desktop manager didn't enforce the position, try to force it again. */

			if (Infowin->x != term_prefs[t_idx].x || Infowin->y != term_prefs[t_idx].y) {
				Infowin_move(term_prefs[t_idx].x, term_prefs[t_idx].y);
			}

			if (td == &term_main) {
				Infowin_set_focus();
			}
			break;

		/* An UnMap Event */
		case UnmapNotify:
			/* Save the mapped-ness */
			Infowin->mapped = 0;
			break;

		/* A Move AND/OR Resize Event */
		case ConfigureNotify:
		{
			/* The windows x and y coordinates are relative to parent window, which doesn't have to be the root window for which it was created.
			 * Window manager (WM) can re-parent/encapsulate the window to/into another internal window for decorations, etc.
			 * Get translated coordinates and take decorations into acount. */
			terminal_window_real_coords_x11(t_idx, &xev->xconfigure.x, &xev->xconfigure.y);

			bool resize = Infowin->w != xev->xconfigure.width || Infowin->h != xev->xconfigure.height;

			/* Save the new Window Parms */
			Infowin->x = xev->xconfigure.x;
			Infowin->y = xev->xconfigure.y;
			Infowin->w = xev->xconfigure.width;
			Infowin->h = xev->xconfigure.height;

			/* The window dimensions in Infowin should be set at the time XResizeWindow is called in this client. */
			/* Therefore if resize is true, then it has to come from other source (eg. user or WM resized the window). */
			if (resize && Infowin->mapped) {
				/* Window resize timer start. */
				gettimeofday(&td->resize_timer, NULL);
				timeval_add_us(&td->resize_timer, 500000); // Add 1/2 second.
			}
			break;
		}
	}


	/* Hack -- Activate the old term */
	Term_activate(&old_td->t);

	/* Hack -- Activate the proper "inner" window */
	Infowin_set(old_td->inner);


	/* XXX XXX Hack -- map/unmap as needed */


	/* Success */
	return(0);
}

#ifdef USE_GRAPHICS
/* Frees all graphics structures in provided term_data and sets them to zero values. */
static void free_graphics(term_data *td) {
	if (td->tiles) {
		XDestroyImage(td->tiles);
		td->tiles = NULL;
	}
	if (td->bgmask) {
		XFreePixmap(Metadpy->dpy, td->bgmask);
		td->bgmask = None;
	}
	if (td->fgmask) {
		XFreePixmap(Metadpy->dpy, td->fgmask);
		td->fgmask = None;
	}
 #ifdef GRAPHICS_BG_MASK
	if (td->bg2mask) {
		XFreePixmap(Metadpy->dpy, td->bg2mask);
		td->bg2mask = None;
	}
	if (td->tilePreparation2) {
		XFreePixmap(Metadpy->dpy, td->tilePreparation2);
		td->tilePreparation2 = None;
	}
 #endif
	if (td->tilePreparation) {
		XFreePixmap(Metadpy->dpy, td->tilePreparation);
		td->tilePreparation = None;
	}
 #ifdef TILE_CACHE_SIZE
	if (!disable_tile_cache)
	for (int i = 0; i < TILE_CACHE_SIZE; i++) {
		if (td->tile_cache[i].tilePreparation) {
			XFreePixmap(Metadpy->dpy, td->tile_cache[i].tilePreparation);
			td->tile_cache[i].tilePreparation = None;
			td->tile_cache[i].c = 0xffffffff;
			td->tile_cache[i].a = 0xff;
  #ifdef GRAPHICS_BG_MASK
			XFreePixmap(Metadpy->dpy, td->tile_cache[i].tilePreparation2);
			td->tile_cache[i].tilePreparation2 = None;
			td->tile_cache[i].c_back = 0xffffffff;
			td->tile_cache[i].a_back = 0xff;
  #endif
			td->tile_cache[i].is_valid = FALSE;
			/* Optional 'bg' and 'fg' need no intialization */
		}
	}
 #endif
}
#endif

/* Closes all X11 windows and frees all allocated data structures for input parameter. */
static errr term_data_nuke(term_data *td) {
	if (td == NULL) return(0);

#ifdef USE_GRAPHICS
	// Free graphics structures.
	if (use_graphics) {
		/* Free graphic tiles & masks. */
		free_graphics(td);
	}
#endif

	// Unmap & free inner window.
	if (td->inner && td->inner->nuke) {
		if (Infowin == td->inner) Infowin_set(NULL);
		if (td->inner->win) {
			XSelectInput(Metadpy->dpy, td->inner->win, 0L);
			XUnmapWindow(Metadpy->dpy, td->inner->win);
			XDestroyWindow(Metadpy->dpy, td->inner->win);
		}
		FREE(td->inner, infowin);
	}

	// Unmap & free outer window.
	if (td->outer && td->outer->nuke) {
		if (Infowin == td->outer) Infowin_set(NULL);
		if (td->outer->win) {
			XSelectInput(Metadpy->dpy, td->outer->win, 0L);
			XUnmapWindow(Metadpy->dpy, td->outer->win);
			XDestroyWindow(Metadpy->dpy, td->outer->win);
		}
		FREE(td->outer, infowin);
	}

	/* Reset timers just to be sure. */
	td->resize_timer.tv_sec=0;
	td->resize_timer.tv_usec=0;

	// Free font.
	if (td->fnt && td->fnt->nuke) {
		if (Infofnt == td->fnt) Infofnt_set(NULL);
		if (td->fnt->info) XFreeFont(Metadpy->dpy, td->fnt->info);
		if (td->fnt->name) string_free(td->fnt->name);
		FREE(td->fnt, infofnt);
	}

	return(0);
}

/* Saves terminal window position, dimensions and font for term_idx to term_prefs.
 * Note: The term_prefs visibility is not handled here. */
static void term_data_to_term_prefs(int term_idx) {
	int cols, rows;

	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;
	term_data *td = term_idx_to_term_data(term_idx);

	/* Update position. */
	term_prefs[term_idx].x = td->outer->x;
	term_prefs[term_idx].y = td->outer->y;

	/* Update dimensions. */
	cols = td->inner->w / td->fnt->wid;
	rows = td->inner->h / td->fnt->hgt;
	term_prefs[term_idx].columns = cols;
	term_prefs[term_idx].lines = rows;

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
static void Term_nuke_x11(term *t) {
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
static errr Term_xtra_x11_level(int v) {
	term_data *td = (term_data*)(Term->data);

	/* Handle "activate" */
	if (v) {
		/* Activate the "inner" window */
		Infowin_set(td->inner);

		/* Activate the "inner" font */
		Infofnt_set(td->fnt);
	}

	/* Success */
	return(0);
}


/*
 * Handle a "special request"
 */
static errr Term_xtra_x11(int n, int v) {
	/* Handle a subset of the legal requests */
	switch (n) {
		/* Make a noise */
		case TERM_XTRA_NOISE: Metadpy_do_beep(); return(0);

		/* Flush the output XXX XXX XXX */
		case TERM_XTRA_FRESH: Metadpy_update(1, 0, 0); return(0);

		/* Process random events XXX XXX XXX */
		case TERM_XTRA_BORED: return(CheckEvent(0));

		/* Process Events XXX XXX XXX */
		case TERM_XTRA_EVENT: return(CheckEvent(v));

		/* Flush the events XXX XXX XXX */
		case TERM_XTRA_FLUSH: while (!CheckEvent(FALSE)); return(0);

		/* Handle change in the "level" */
		case TERM_XTRA_LEVEL: return(Term_xtra_x11_level(v));

		/* Clear the screen */
		case TERM_XTRA_CLEAR: Infowin_wipe(); return(0);

		/* Delay for some milliseconds */
		case TERM_XTRA_DELAY: usleep(1000 * v); return(0);
	}

	/* Unknown */
	return(1);
}


/*
 * Erase a number of characters
 */
static errr Term_wipe_x11(int x, int y, int n) {
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
static errr Term_curs_x11(int x, int y) {
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
static errr Term_text_x11(int x, int y, int n, byte a, cptr s) {
	/* Catch use in chat instead of as feat attr, or we crash :-s
	   (term-idx 0 is the main window; screen-pad-left check: In case it is used in the status bar for some reason; screen-pad-top checks: main screen top chat line or status line) */
	if (Term && Term->data == &term_main && x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + screen_wid && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + screen_hgt) {
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


#ifdef USE_GRAPHICS /* huge block */

/* Directory with graphics tiles files (should be lib/xtra/grapics). */
static cptr ANGBAND_DIR_XTRA_GRAPHICS;
/* Loaded tiles image and masks. */
XImage *graphics_image = None;
char *graphics_bgmask = NULL;
char *graphics_fgmask = NULL;
 #ifdef GRAPHICS_BG_MASK
char *graphics_bg2mask = NULL;
 #endif
/* These variables are computed at image load (in 'init_x11'). */
int graphics_tile_wid, graphics_tile_hgt;
int graphics_image_tpr; /* Tiles per row. */

/*
 * Draw some graphical characters.
 */
static errr Term_pict_x11(int x, int y, byte a, char32_t c) {
	term_data *td;
	Pixmap tilePreparation;
 #ifdef TILE_CACHE_SIZE
	struct tile_cache_entry *entry;
	int i, hole = -1;
 #endif
	int x1, y1;

	/* Catch use in chat instead of as feat attr, or we crash :-s
	   (term-idx 0 is the main window; screen-pad-left check: In case it is used in the status bar for some reason; screen-pad-top checks: main screen top chat line or status line) */
	if (Term && Term->data == &term_main && x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + screen_wid && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + screen_hgt) {
		flick_global_x = x;
		flick_global_y = y;
	} else flick_global_x = 0;

	a = term2attr(a);

	/* Draw the tile in Xor. */
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

	if (Infoclr->fg == Infoclr->bg) {
		/* Foreground color is the same as background color. If this was text, the tile would be rendered as solid block of color.
		 * But an image tile could contain some other color pixels and could result in no solid color tile.
		 * That's why paint a solid block as intended. */
		return(Infofnt_text_std(x, y, " ", 1));
	}

	td = (term_data*)(Term->data);
	x *= Infofnt->wid;
	y *= Infofnt->hgt;

 #ifdef TILE_CACHE_SIZE
	if (!disable_tile_cache) {
		entry = NULL;
		for (i = 0; i < TILE_CACHE_SIZE; i++) {
			entry = &td->tile_cache[i];
			if (!entry->is_valid) hole = i;
			else if (entry->c == c && entry->a == a
  #ifdef GRAPHICS_BG_MASK
			    && entry->c_back == 0 && entry->a_back == 0
  #endif
  #ifdef TILE_CACHE_FGBG /* Instead of this, invalidate_graphics_cache_...() will specifically invalidate affected entries */
			    /* Extra: Verify that palette is identical - allows palette_animation to work w/o invalidating the whole cache each time: */
			    && Infoclr->fg == entry->fg && Infoclr->bg == entry->bg
  #endif
			    ) {
				/* Copy cached tile to window. */
				XCopyArea(Metadpy->dpy, entry->tilePreparation, td->inner->win, Infoclr->gc,
					0, 0,
					td->fnt->wid, td->fnt->hgt,
					x, y);

				/* Success */
				return(0);
			}
		}

		// Replace invalid cache entries right away in-place, so we don't kick other still valid entries out via FIFO'ing
		if (hole != -1) {
  #ifdef TILE_CACHE_LOG
			c_msg_format("Tile cache pos (hole): %d / %d", hole, TILE_CACHE_SIZE);
  #endif
			entry = &td->tile_cache[hole];
		} else {
  #ifdef TILE_CACHE_LOG
			c_msg_format("Tile cache pos (FIFO): %d / %d", td->cache_position, TILE_CACHE_SIZE);
  #endif
			// Replace valid cache entries in FIFO order
			entry = &td->tile_cache[td->cache_position++];
			if (td->cache_position >= TILE_CACHE_SIZE) td->cache_position = 0;
		}

		tilePreparation = entry->tilePreparation;
		entry->c = c;
		entry->a = a;
  #ifdef GRAPHICS_BG_MASK
		entry->c_back = 0;
		entry->a_back = 0;
  #endif
		entry->is_valid = TRUE;
  #ifdef TILE_CACHE_FGBG
		entry->fg = Infoclr->fg;
		entry->bg = Infoclr->bg;
  #endif
	} else tilePreparation = td->tilePreparation;
 #else /* (TILE_CACHE_SIZE) No caching: */
	tilePreparation = td->tilePreparation;
 #endif

	/* Prepare tile to preparation pixmap. */
	x1 = ((c - MAX_FONT_CHAR - 1) % graphics_image_tpr) * td->fnt->wid;
	y1 = ((c - MAX_FONT_CHAR - 1) / graphics_image_tpr) * td->fnt->hgt;
	XCopyPlane(Metadpy->dpy, td->fgmask, tilePreparation, Infoclr->gc,
		   x1, y1,
		   td->fnt->wid, td->fnt->hgt,
		   0, 0,
		   1);
	XSetClipMask(Metadpy->dpy, Infoclr->gc, td->bgmask);
	XSetClipOrigin(Metadpy->dpy, Infoclr->gc, 0 - x1, 0 - y1);
	XPutImage(Metadpy->dpy, tilePreparation,
		  Infoclr->gc,
		  td->tiles,
		  x1, y1,
		  0, 0,
		  td->fnt->wid, td->fnt->hgt);
	XSetClipMask(Metadpy->dpy, Infoclr->gc, None);

	/* Copy prepared tile to window. */
	XCopyArea(Metadpy->dpy, tilePreparation, td->inner->win, Infoclr->gc,
		  0, 0,
		  td->fnt->wid, td->fnt->hgt,
		  x, y);

	/* Success */
	return(0);
}
 #ifdef GRAPHICS_BG_MASK
static errr Term_pict_x11_2mask(int x, int y, byte a, char32_t c, byte a_back, char32_t c_back) {
  #if 0 /* use fallback hook until 2mask routines are complete? */
	return(Term_pict_x11(x, y, a, c));
  #else
	term_data *td;
	Pixmap tilePreparation, tilePreparation2;
   #ifdef TILE_CACHE_SIZE
	struct tile_cache_entry *entry;
	int i, hole = -1;
   #endif
	int x1, y1;

	/* SPACE = erase background, aka black background. This is for places where we have no bg-info, such as client-lore in knowledge menu. */
	if (c_back == 32) a_back = TERM_DARK;

	/* Catch use in chat instead of as feat attr, or we crash :-s
	   (term-idx 0 is the main window; screen-pad-left check: In case it is used in the status bar for some reason; screen-pad-top checks: main screen top chat line or status line) */
	if (Term && Term->data == &term_main && x >= SCREEN_PAD_LEFT && x < SCREEN_PAD_LEFT + screen_wid && y >= SCREEN_PAD_TOP && y < SCREEN_PAD_TOP + screen_hgt) {
		flick_global_x = x;
		flick_global_y = y;
	} else flick_global_x = 0;

	a = term2attr(a);
	a_back = term2attr(a_back);

	/* Draw the tile in Xor. */
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

	if (Infoclr->fg == Infoclr->bg) {
		/* Foreground color is the same as background color. If this was text, the tile would be rendered as solid block of color.
		 * But an image tile could contain some other color pixels and could result in no solid color tile.
		 * That's why paint a solid block as intended. */
		return(Infofnt_text_std(x, y, " ", 1));
	}

	td = (term_data*)(Term->data);
	x *= Infofnt->wid;
	y *= Infofnt->hgt;

   #ifdef TILE_CACHE_SIZE
    if (!disable_tile_cache) {
	entry = NULL;
	for (i = 0; i < TILE_CACHE_SIZE; i++) {
		entry = &td->tile_cache[i];
		if (!entry->is_valid) hole = i;
		else if (entry->c == c && entry->a == a && entry->c_back == c_back && entry->a_back == a_back
    #ifdef TILE_CACHE_FGBG /* Instead of this, invalidate_graphics_cache_...() will specifically invalidate affected entries */
		    /* Extra: Verify that palette is identical - allows palette_animation to work w/o invalidating the whole cache each time: */
		    && Infoclr->fg == entry->fg && Infoclr->bg == entry->bg
    #endif
		    ) {
			/* Copy cached tile to window. */
			XCopyArea(Metadpy->dpy, entry->tilePreparation2, td->inner->win, Infoclr->gc, // NOTE that tilePreparation2 holds the final tile, NOT tilePreparation!
				0, 0,
				td->fnt->wid, td->fnt->hgt,
				x, y);

			/* Success */
			return(0);
		}
	}

	// Replace invalid cache entries right away in-place, so we don't kick other still valid entries out via FIFO'ing
	if (hole != -1) {
    #ifdef TILE_CACHE_LOG
		c_msg_format("Tile cache pos (hole): %d / %d", hole, TILE_CACHE_SIZE);
    #endif
		entry = &td->tile_cache[hole];
	} else {
    #ifdef TILE_CACHE_LOG
		c_msg_format("Tile cache pos (FIFO): %d / %d", td->cache_position, TILE_CACHE_SIZE);
    #endif
		// Replace valid cache entries in FIFO order
		entry = &td->tile_cache[td->cache_position++];
		if (td->cache_position >= TILE_CACHE_SIZE) td->cache_position = 0;
	}

	tilePreparation = entry->tilePreparation; //in 2mask-mode actually not used, as only tilePreparation2 is interesting as it holds the final result
	tilePreparation2 = entry->tilePreparation2;
	entry->c = c;
	entry->a = a;
    #ifdef GRAPHICS_BG_MASK
	entry->c_back = c_back;
	entry->a_back = a_back;
    #endif
	entry->is_valid = TRUE;
    #ifdef TILE_CACHE_FGBG
	entry->fg = Infoclr->fg;
	entry->bg = Infoclr->bg;
    #endif
    } else {
	tilePreparation = td->tilePreparation;
	tilePreparation2 = td->tilePreparation2;
    }
   #else /* (TILE_CACHE_SIZE) No caching: */
	tilePreparation = td->tilePreparation;
	tilePreparation2 = td->tilePreparation2;
   #endif

	/* Start with background tile to preparation pixmap,
	   so the background mask can be reverse AND'ed (draw bg where there's 0x000000 in the foreground)
	   onto the background image to zero out all pixels that should display the foreground instead.
	   After that, the foreground can be OR'd onto the image. - C. Blue */

	/* Switch fgmask'ing colour to background-tile colour */
   #ifndef EXTENDED_COLOURS_PALANIM
    #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a_back & 0x0F]);
    #else
	Infoclr_set(clr[a_back & 0x1F]); //undefined case actually, we don't want to have a hole in the colour array (0..15 and then 32..32+x) -_-
    #endif
   #else
    #ifndef EXTENDED_BG_COLOURS
	Infoclr_set(clr[a_back & 0x1F]);
    #else
	Infoclr_set(clr[a_back & 0x3F]);
    #endif
   #endif

	/* Prepare background tile to preparation pixmap. +chopchop+ */
	if (c_back == 32) {
		/* hack: SPACE aka ASCII 32 means empty background ie fill in a_back colour */
		XFillRectangle(Metadpy->dpy, tilePreparation2, Infoclr->gc, 0, 0, td->fnt->wid, td->fnt->hgt);
	} else {
		x1 = ((c_back - MAX_FONT_CHAR - 1) % graphics_image_tpr) * td->fnt->wid;
		y1 = ((c_back - MAX_FONT_CHAR - 1) / graphics_image_tpr) * td->fnt->hgt;
		XCopyPlane(Metadpy->dpy, td->fgmask, tilePreparation2, Infoclr->gc,
			   x1, y1,
			   td->fnt->wid, td->fnt->hgt,
			   0, 0,
			   1);
		XSetClipMask(Metadpy->dpy, Infoclr->gc, td->bgmask);
		XSetClipOrigin(Metadpy->dpy, Infoclr->gc, 0 - x1, 0 - y1);
		XPutImage(Metadpy->dpy, tilePreparation2,
			  Infoclr->gc,
			  td->tiles,
			  x1, y1,
			  0, 0,
			  td->fnt->wid, td->fnt->hgt);
		XSetClipMask(Metadpy->dpy, Infoclr->gc, None);
	}

	/* Revert fgmask'ing colour to foreground-tile colour */
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

	/* Prepare foreground tile to preparation pixmap. */
	x1 = ((c - MAX_FONT_CHAR - 1) % graphics_image_tpr) * td->fnt->wid;
	y1 = ((c - MAX_FONT_CHAR - 1) / graphics_image_tpr) * td->fnt->hgt;
	XCopyPlane(Metadpy->dpy, td->fgmask, tilePreparation, Infoclr->gc,
		   x1, y1,
		   td->fnt->wid, td->fnt->hgt,
		   0, 0,
		   1);
	XSetClipMask(Metadpy->dpy, Infoclr->gc, td->bgmask);
	XSetClipOrigin(Metadpy->dpy, Infoclr->gc, 0 - x1, 0 - y1);
	XPutImage(Metadpy->dpy, tilePreparation,
		  Infoclr->gc,
		  td->tiles,
		  x1, y1,
		  0, 0,
		  td->fnt->wid, td->fnt->hgt);
	XSetClipMask(Metadpy->dpy, Infoclr->gc, None);

   #if 1
	/* Finally copy foreground tile onto background tile, via bg2mask. */
	XSetClipMask(Metadpy->dpy, Infoclr->gc, td->bg2mask);
	XSetClipOrigin(Metadpy->dpy, Infoclr->gc, 0 - x1, 0 - y1);
	//XPutImage(Metadpy->dpy, tilePreparation, Infoclr->gc, tilePreparation2, 0, 0, 0, 0, td->fnt->wid, td->fnt->hgt);
	XCopyArea(Metadpy->dpy, tilePreparation, tilePreparation2, Infoclr->gc, 0, 0, td->fnt->wid, td->fnt->hgt, 0, 0);	// NOTE that tilePreparation2 holds the final tile, NOT tilePreparation! (Compare tile-caching!)
	XSetClipMask(Metadpy->dpy, Infoclr->gc, None);
   #endif

	/* Copy prepared combo-tile to window. */
	XCopyArea(Metadpy->dpy, tilePreparation2, td->inner->win, Infoclr->gc,
		  0, 0,
		  td->fnt->wid, td->fnt->hgt,
		  x, y);

	/* Success */
	return(0);
  #endif
}
 #endif /* GRAPHICS_BG_MASK */
 #ifdef TILE_CACHE_SIZE
/* c_idx: -1 = invalidate all; otherwise only tiles that use this colour are invalidated. */
static void invalidate_graphics_cache_x11(term_data *td, int c_idx) {
	int i;

	if (disable_tile_cache) return;

	if (c_idx == -1)
		for (i = 0; i < TILE_CACHE_SIZE; i++)
			td->tile_cache[i].is_valid = FALSE;
	else
		for (i = 0; i < TILE_CACHE_SIZE; i++)
			if (td->tile_cache[i].a == c_idx
  #ifdef GRAPHICS_BG_MASK
			    || td->tile_cache[i].a_back == c_idx
  #endif
			    )
				td->tile_cache[i].is_valid = FALSE;
}
 #endif

/* Salvaged and adapted from http://www.phial.com/angdirs/angband-291/src/maid-x11.c */
/*
 * Hack -- Convert an RGB value to an X11 Pixel, or die.
 */
static unsigned long create_pixel(Display *dpy, byte red, byte green, byte blue) {
	Colormap cmap = DefaultColormapOfScreen(DefaultScreenOfDisplay(dpy));

	char cname[8];

	XColor xcolour;

	/* Build the color. */
	xcolour.red = red * 255;
	xcolour.green = green * 255;
	xcolour.blue = blue * 255;
	xcolour.flags = DoRed | DoGreen | DoBlue;

	/* Attempt to Allocate the Parsed color. */
	if (!(XAllocColor(dpy, cmap, &xcolour)))
		quit_fmt("Couldn't allocate bitmap color '%s'\n", cname);

	return(xcolour.pixel);
}

/*
 * The Win32 "BITMAPFILEHEADER" type.
 *
 * Note the "bfAlign" field, which is a complete hack to ensure that the
 * "u32b" fields in the structure get aligned.  Thus, when reading this
 * header from the file, we must be careful to skip this field.
 */
typedef struct BITMAPFILEHEADER {
	u16b bfAlign;     // Align bits, to have not have 16 bits alone.
	u16b bfType;      // File type always BM which is 0x4D42 (19778).
	u32b bfSize;      // Size of the file (in bytes).
	u16b bfReserved1; // Reserved, always 0.
	u16b bfReserved2; // Reserved, always 0.
	u32b bfOffBytes;  // Start position of pixel data (bytes from the beginning of the file).
} BITMAPFILEHEADER;

/*
 * The Win32 "BITMAPINFOHEADER" type.
 */
typedef struct BITMAPINFOHEADER {
	u32b biSize;           // Size of this header (in bytes).
	s32b biWidth;          // width of bitmap in pixels.
	s32b biHeight;         // height of bitmap in pixels (if positive, bottom-up, with origin in lower left corner, if negative, top-down, with origin in upper left corner).
	u16b biPlanes;         // No. of planes for the target device, this is always 1.
	u16b biBitCount;       // No. of bits per pixel.
	u32b biCompresion;     // 0 or 3 - uncompressed. THIS PROGRAM CONSIDERS ONLY UNCOMPRESSED BMP images.
	u32b biSizeImage;      // 0 - for uncompressed images.
	u32b biXPelsPerMeter;
	u32b biYPelsPerMeter;
	u32b biClrUsed;        // No. color indexes in the color table. Use 0 for the max number of colors allowed by bit_count.
	u32b biClrImportant;
} BITMAPINFOHEADER;

/*
 * The Win32 "RGBQUAD" type.
 */
typedef struct RGBQUAD {
	unsigned char r,g,b;
	unsigned char filler;
} RGBQUAD;

/* ReadBMPData errors */
#define ReadBMPNoFile				-1
#define ReadBMPInvalidFile			-2
#define ReadBMPNoImageData			-3
#define ReadBMPUnexpectedEOF			-4
#define ReadBMPReadErrorOrUnexpectedEOF		-5
#define ReadBMPIllegalBitCount			-6

/*
 * Read a Win32 BMP file into data_return variable and
 * sets width_return and height_return according to readed image dimensions.
 *
 * Currently only handles bitmaps with 24bit or 32bit per color.
 *
 * The data_return consists of width_return*height_return pixels.
 * Each returned pixel consists of 4*8bit, in format BGRA, where A is always 0.
 * Note: Xlib works with BGR colors intead of RGB.
 *
 * It's your responsibility to free data_return after usage.
 * Function will not free memory if allready allocated in data_return input variable.
 */
static errr ReadBMPData(char *Name, char **data_return,  int *width_return, int *height_return) {
	FILE *f;
	BITMAPFILEHEADER fileheader;
	BITMAPINFOHEADER infoheader;
	vptr fileheaderhack = (vptr)((char *)(&fileheader) + sizeof(fileheader.bfAlign));

	/* Open the BMP file. */
	if (NULL == (f = fopen(Name, "r")))
		/* No such file. */
		return(ReadBMPNoFile);

	/* Read the "BITMAPFILEHEADER". */
	if (1 != fread(fileheaderhack, sizeof(fileheader) - sizeof(fileheader.bfAlign), 1, f)) {fclose(f); return(ReadBMPReadErrorOrUnexpectedEOF);}
	/* Read the "BITMAPINFOHEADER". */
	if (1 != fread(&infoheader, sizeof(infoheader), 1, f)) {fclose(f); return(ReadBMPReadErrorOrUnexpectedEOF);}
	/* Verify. */
	if (feof(f) || fileheader.bfType != 19778) {fclose(f); return(ReadBMPInvalidFile);}
	if (infoheader.biBitCount != 24 && infoheader.biBitCount != 32) {fclose(f); return(ReadBMPIllegalBitCount);}
	if (infoheader.biWidth * infoheader.biHeight == 0) {fclose(f); return(ReadBMPNoImageData);}
	/* Position file read head to image data. */
	if (0 != fseek(f, fileheader.bfOffBytes, SEEK_SET)) {fclose(f); return(ReadBMPUnexpectedEOF);}

	char *data;
	C_MAKE(data, infoheader.biWidth*infoheader.biHeight * 4, char);
	memset(data, 0, infoheader.biWidth*infoheader.biHeight * 4);
	errr err = 0;

	/* Every line is padded, to have multiple o 4 bytes. */
	int linePadding = (4 - (3 * infoheader.biWidth) % 4) % 4;

	for (int n = 0; err == 0 && n < abs(infoheader.biHeight); n++) {
		int y = infoheader.biHeight - n - 1;

		if (infoheader.biHeight < 0) y = n;

		for (int x = 0; x < infoheader.biWidth; x++) {
			int i = 4 * (y * infoheader.biWidth + x);

			/* Usually the pixel colors are in BGR (or BGRA) order. The order can be different,
			 * depending on header info, but for simplicity assume BGR (or BGRA) ordering. */
			if (1 != fread(&data[i], 3, 1, f)) {err = ReadBMPUnexpectedEOF; break;}
			if (infoheader.biBitCount == 32) {
				/* The format can be BGRA or BGRX, anyway skip last byte (A or X component). */
				if (0 != fseek(f, 1, SEEK_CUR)) {fclose(f); return(ReadBMPUnexpectedEOF);}
			}
		}
		/* Adjust read head if padding. */
		if (linePadding > 0)
			if (0 != fseek(f, linePadding, SEEK_CUR)) {err = ReadBMPUnexpectedEOF; break;}
	}
	fclose(f);

	if (err != 0) {
		C_KILL(data, infoheader.biWidth*infoheader.biHeight*4, char);
		return(err);
	}

	(*width_return) = infoheader.biWidth;
	(*height_return) = abs(infoheader.biHeight);
	(*data_return) = data;
	return(0);
}

/*
 * Creates 1bit per pixel background and foreground masks.
 * Foreground mask (fgmask_return) determines which pixels in image will be drawn with character color.
 * Background mask (bgmask_return) determines pixels, which will be and not be drawn at all.
 * Pixel in fgmask_return is 1, only if image color on the position is magenta (#ff00ff).
 * Pixel in bgmask_return is 1, only if image color is not black (#000000), nor magenta (#ff00ff).
 * Function will not free memory if allready allocated in bgmask_return/fgmask_return input variable.
 */
 #ifndef GRAPHICS_BG_MASK
static void createMasksFromData(char* data, int width, int height, char **bgmask_return, char **fgmask_return) {
	int masks_size = width * height / 8 + (width * height % 8 == 0 ? 0 : 1);
	u32b bit;
	byte r, g, b;
	int x, y;

	char *bgmask;
	C_MAKE(bgmask, masks_size, char);
	memset(bgmask, 0, masks_size);

	char *fgmask;
	C_MAKE(fgmask, masks_size, char);
	memset(fgmask, 0, masks_size);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			bit = y * width + x;
			b = data[4 * (x + y * width)];
			g = data[4 * (x + y * width) + 1];
			r = data[4 * (x + y * width) + 2];

			/* Ensure non-GRAPHICS_BG_MASK backward compatibility with 2mask-ready tilesets that use the dual-mask colour! */
			if (r == GFXMASK_BG2_R && g == GFXMASK_BG2_G && b == GFXMASK_BG2_B) {
				b = data[4 * (x + y * width)] = GFXMASK_BG_B;
				g = data[4 * (x + y * width) + 1] = GFXMASK_BG_G;
				r = data[4 * (x + y * width) + 2] = GFXMASK_BG_R;
			}

			if (r != GFXMASK_BG_R || g != GFXMASK_BG_G || b != GFXMASK_BG_B)
				bgmask[bit / 8] |= 1 << (bit % 8);

			//todo:implement -> if ((GFXMASK_FG_R == -1 || r == GFXMASK_FG_R) && (GFXMASK_FG_G == -1 || g == GFXMASK_FG_G) && (GFXMASK_FG_B == -1 || b == GFXMASK_FG_B)) {
			if (r == GFXMASK_FG_R && g == GFXMASK_FG_G && b == GFXMASK_FG_B) {
				fgmask[bit / 8] |= 1 << (bit % 8);
				bgmask[bit / 8] &= ~((char)1 << (bit % 8));
			}
		}
	}

	(*bgmask_return) = bgmask;
	(*fgmask_return) = fgmask;
}
 #else
static void createMasksFromData_2mask(char* data, int width, int height, char **bgmask_return, char **fgmask_return, char **bg2mask_return) {
//#define BG2MASK_INV /* Have '1' on black foreground tile area (instead of coloured foreground tile area)? */
	int masks_size = width * height / 8 + (width * height % 8 == 0 ? 0 : 1);
	u32b bit, pixel;
	byte r, g, b;
	int x, y;

	char *bgmask;
	C_MAKE(bgmask, masks_size, char);
	memset(bgmask, 0, masks_size);

	char *fgmask;
	C_MAKE(fgmask, masks_size, char);
	memset(fgmask, 0, masks_size);

	char *bg2mask;
	C_MAKE(bg2mask, masks_size, char);
  #ifdef BG2MASK_INV
	memset(bg2mask, 1, masks_size);
  #else
	memset(bg2mask, 0, masks_size);
  #endif

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			bit = y * width + x;
			pixel = 4 * bit;
			b = data[pixel];
			g = data[pixel + 1];
			r = data[pixel + 2];

			/* We're not in dual-mask mode? Translate 2mask pixels back to normal bgmask: */
			if (use_graphics != UG_2MASK &&
			    r == GFXMASK_BG2_R && g == GFXMASK_BG2_G && b == GFXMASK_BG2_B) {
				b = data[pixel] = GFXMASK_BG_B;
				g = data[pixel + 1] = GFXMASK_BG_G;
				r = data[pixel + 2] = GFXMASK_BG_R;
			}

			if (r != GFXMASK_BG_R || g != GFXMASK_BG_G || b != GFXMASK_BG_B)
				bgmask[bit / 8] |= 1 << (bit % 8);

			if (r != GFXMASK_BG2_R || g != GFXMASK_BG2_G || b != GFXMASK_BG2_B)
  #ifdef BG2MASK_INV
				bg2mask[bit / 8] &= ~((char)1 << (bit % 8));
  #else
				bg2mask[bit / 8] |= 1 << (bit % 8);
  #endif

			//todo:implement -> if ((GFXMASK_FG_R == -1 || r == GFXMASK_FG_R) && (GFXMASK_FG_G == -1 || g == GFXMASK_FG_G) && (GFXMASK_FG_B == -1 || b == GFXMASK_FG_B)) {
			if (r == GFXMASK_FG_R && g == GFXMASK_FG_G && b == GFXMASK_FG_B) {
				fgmask[bit / 8] |= 1 << (bit % 8);
				bgmask[bit / 8] &= ~((char)1 << (bit % 8));
			}
		}
	}

	(*bgmask_return) = bgmask;
	(*fgmask_return) = fgmask;
	(*bg2mask_return) = bg2mask;
}
 #endif

/*
 * Resize an image. XXX XXX XXX
 *
 * Added bg/fg masks resizing.
 * It's your responsibility to free returned XImage, bgmask_return and fgmask_return after usage.
 * Function will not free memory if already allocated in bgmask_return or fgmask_return input variable.
 */
 #ifndef GRAPHICS_BG_MASK
static XImage *ResizeImage(Display *disp, XImage *Im,
                           int ix, int iy, int ox, int oy,
                           char *bgbits, char *fgbits, Pixmap *bgmask_return, Pixmap *fgmask_return) {
	int width1, height1, width2, height2;
	int x1, x2, y1, y2, Tx, Ty;
	int *px1, *px2, *dx1, *dx2;
	int *py1, *py2, *dy1, *dy2;

	XImage *Tmp;
	char *Data;


	width1 = Im->width;
	height1 = Im->height;

	width2 = ox * width1 / ix;
	height2 = oy * height1 / iy;

	Data = (char *)malloc(width2 * height2 * Im->bits_per_pixel / 8);

	Tmp = XCreateImage(
			disp, DefaultVisual(disp, DefaultScreen(disp)), Im->depth, ZPixmap, 0,
			Data, width2, height2, Im->bits_per_pixel, 0);


	int linePadBits = 8;
	int paddedWidth2 = width2 + ((linePadBits - (width2 % linePadBits)) % linePadBits);
	int new_masks_size = paddedWidth2 * height2 / 8;
	char *bgmask_data;
	C_MAKE(bgmask_data, new_masks_size, char);
	memset(bgmask_data, 0, new_masks_size);

	char *fgmask_data;
	C_MAKE(fgmask_data, new_masks_size, char);
	memset(fgmask_data, 0, new_masks_size);

	if (ix >= ox) {
		px1 = &x1;
		px2 = &x2;
		dx1 = &ix;
		dx2 = &ox;
	} else {
		px1 = &x2;
		px2 = &x1;
		dx1 = &ox;
		dx2 = &ix;
	}

	if (iy >= oy) {
		py1 = &y1;
		py2 = &y2;
		dy1 = &iy;
		dy2 = &oy;
	} else {
		py1 = &y2;
		py2 = &y1;
		dy1 = &oy;
		dy2 = &iy;
	}

	Ty = *dy1;

	for (y1 = 0, y2 = 0; (y1 < height1) && (y2 < height2); ) { /* Wrong compiler warning, the loop vars _are_ modified via px/dx/py/dy */
		Tx = *dx1;

		for (x1 = 0, x2 = 0; (x1 < width1) && (x2 < width2); ) { /* Wrong compiler warning, the loop vars _are_ modified via px/dx/py/dy */
			XPutPixel(Tmp, x2, y2, XGetPixel(Im, x1, y1));
			u32b maskbitno = (x1 + (y1 * width1));
			u32b newmaskbitno = (x2 + (y2 * paddedWidth2));
			bool bgbit = bgbits[maskbitno / 8] & (1 << (maskbitno % 8));

			if (bgbit) bgmask_data[newmaskbitno / 8] |= 1 << (newmaskbitno % 8);
			else bgmask_data[newmaskbitno / 8] &= ~(1 << (newmaskbitno % 8));

			bool fgbit = fgbits[maskbitno / 8] & (1 << (maskbitno % 8));

			if (fgbit) fgmask_data[newmaskbitno / 8] |= 1 << (newmaskbitno % 8);
			else fgmask_data[newmaskbitno / 8] &= ~(1 << (newmaskbitno % 8));

			(*px1)++;

			Tx -= *dx2;
			if (Tx <= 0) {
				Tx += *dx1;
				(*px2)++;
			}
		}

		(*py1)++;

		Ty -= *dy2;
		if (Ty <= 0) {
			Ty += *dy1;
			(*py2)++;
		}
	}

	Window root_win = DefaultRootWindow(disp);
	(*bgmask_return) = XCreateBitmapFromData(disp, root_win, bgmask_data, width2, height2);
	(*fgmask_return) = XCreateBitmapFromData(disp, root_win, fgmask_data, width2, height2);
	return(Tmp);
}
 #else
static XImage *ResizeImage_2mask(Display *disp, XImage *Im,
                           int ix, int iy, int ox, int oy,
                           char *bgbits, char *fgbits, char *bg2bits, Pixmap *bgmask_return, Pixmap *fgmask_return, Pixmap *bg2mask_return) {
	int width1, height1, width2, height2;
	int x1, x2, y1, y2, Tx, Ty;
	int *px1, *px2, *dx1, *dx2;
	int *py1, *py2, *dy1, *dy2;

	XImage *Tmp;
	char *Data;


	width1 = Im->width;
	height1 = Im->height;

	width2 = ox * width1 / ix;
	height2 = oy * height1 / iy;

	Data = (char *)malloc(width2 * height2 * Im->bits_per_pixel / 8);

	Tmp = XCreateImage(
			disp, DefaultVisual(disp, DefaultScreen(disp)), Im->depth, ZPixmap, 0,
			Data, width2, height2, Im->bits_per_pixel, 0);


	int linePadBits = 8;
	int paddedWidth2 = width2 + ((linePadBits - (width2 % linePadBits)) % linePadBits);
	int new_masks_size = paddedWidth2 * height2 / 8;

	char *bgmask_data;
	C_MAKE(bgmask_data, new_masks_size, char);
	memset(bgmask_data, 0, new_masks_size);

	char *fgmask_data;
	C_MAKE(fgmask_data, new_masks_size, char);
	memset(fgmask_data, 0, new_masks_size);

	char *bg2mask_data;
	C_MAKE(bg2mask_data, new_masks_size, char);
	memset(bg2mask_data, 0, new_masks_size);

	if (ix >= ox) {
		px1 = &x1;
		px2 = &x2;
		dx1 = &ix;
		dx2 = &ox;
	} else {
		px1 = &x2;
		px2 = &x1;
		dx1 = &ox;
		dx2 = &ix;
	}

	if (iy >= oy) {
		py1 = &y1;
		py2 = &y2;
		dy1 = &iy;
		dy2 = &oy;
	} else {
		py1 = &y2;
		py2 = &y1;
		dy1 = &oy;
		dy2 = &iy;
	}

	Ty = *dy1;

	for (y1 = 0, y2 = 0; (y1 < height1) && (y2 < height2); ) { /* Wrong compiler warning, the loop vars _are_ modified via px/dx/py/dy */
		Tx = *dx1;

		for (x1 = 0, x2 = 0; (x1 < width1) && (x2 < width2); ) { /* Wrong compiler warning, the loop vars _are_ modified via px/dx/py/dy */
			XPutPixel(Tmp, x2, y2, XGetPixel(Im, x1, y1));
			u32b maskbitno = (x1 + (y1 * width1));
			u32b newmaskbitno = (x2 + (y2 * paddedWidth2));

			bool bgbit = bgbits[maskbitno / 8] & (1 << (maskbitno % 8));

			if (bgbit) bgmask_data[newmaskbitno / 8] |= 1 << (newmaskbitno % 8);
			else bgmask_data[newmaskbitno / 8] &= ~(1 << (newmaskbitno % 8));

			bool fgbit = fgbits[maskbitno / 8] & (1 << (maskbitno % 8));

			if (fgbit) fgmask_data[newmaskbitno / 8] |= 1 << (newmaskbitno % 8);
			else fgmask_data[newmaskbitno / 8] &= ~(1 << (newmaskbitno % 8));

			bool bg2bit = bg2bits[maskbitno / 8] & (1 << (maskbitno % 8));

			if (bg2bit) bg2mask_data[newmaskbitno / 8] |= 1 << (newmaskbitno % 8);
			else bg2mask_data[newmaskbitno / 8] &= ~(1 << (newmaskbitno % 8));

			(*px1)++;

			Tx -= *dx2;
			if (Tx <= 0) {
				Tx += *dx1;
				(*px2)++;
			}
		}

		(*py1)++;

		Ty -= *dy2;
		if (Ty <= 0) {
			Ty += *dy1;
			(*py2)++;
		}
	}

	Window root_win = DefaultRootWindow(disp);
	(*bgmask_return) = XCreateBitmapFromData(disp, root_win, bgmask_data, width2, height2);
	(*fgmask_return) = XCreateBitmapFromData(disp, root_win, fgmask_data, width2, height2);
	(*bg2mask_return) = XCreateBitmapFromData(disp, root_win, bg2mask_data, width2, height2);
	return(Tmp);
}
 #endif

#endif /* USE_GRAPHICS */



/*
 * Initialize a term_data
 */
static errr term_data_init(int index, term_data *td, bool fixed, cptr name, cptr font) {
	term *t = &td->t;

	int wid, hgt, num;
	int win_cols, win_lines, wid_outer, hgt_outer;
	cptr n;
	int topx, topy; /* 0, 0 default */


	/* Use values from .tomenetrc;
	   Environment variables (see further below) may override those. */
	win_cols = term_prefs[index].columns;
	win_lines = term_prefs[index].lines;
	topx = term_prefs[index].x;
	topy = term_prefs[index].y;

	/* Prepare the standard font */
	MAKE(td->fnt, infofnt);
	infofnt *old_infofnt = Infofnt;
	Infofnt_set(td->fnt);
	if (Infofnt_init_data(font) == -1) {
		/* Initialization failed, log and try to use the default font. */
		fprintf(stderr, "Failed to load the \"%s\" font for terminal %d\n", font, index);
		if (in_game) {
			/* If in game, inform the user. */
			Infofnt_set(old_infofnt);
			plog_fmt("Failed to load the \"%s\" font! Falling back to default font.\n", font);
			Infofnt_set(td->fnt);
		} 
		if (Infofnt_init_data(x11_terms_font_default[index]) == -1) {
			/* Initialization of the default font failed too. Log, free allocated memory and return with error. */
			fprintf(stderr, "Failed to load the default \"%s\" font for terminal %d\n", x11_terms_font_default[index], index);
			Infofnt_set(old_infofnt);
			if (in_game) {
				/* If in game, inform the user. */
				plog_fmt("Failed to load the default \"%s\" font too! Try to change font manually.\n", x11_terms_font_default[index]);
			}
			FREE(td->fnt, infofnt);
			return(1);
		}
	}

	/* Hack -- extract key buffer size */
	num = (fixed ? 1024 : 16);

	if (!strcmp(name, ang_term_name[0])) {
		n = getenv("TOMENET_X11_WID_TERM_MAIN");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_MAIN");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[1])) {
		n = getenv("TOMENET_X11_WID_TERM_1");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_1");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[2])) {
		n = getenv("TOMENET_X11_WID_TERM_2");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_2");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[3])) {
		n = getenv("TOMENET_X11_WID_TERM_3");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_3");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[4])) {
		n = getenv("TOMENET_X11_WID_TERM_4");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_4");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[5])) {
		n = getenv("TOMENET_X11_WID_TERM_5");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_5");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[6])) {
		n = getenv("TOMENET_X11_WID_TERM_6");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_6");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[7])) {
		n = getenv("TOMENET_X11_WID_TERM_7");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_7");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[8])) {
		n = getenv("TOMENET_X11_WID_TERM_8");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_8");
		if (n) win_lines = atoi(n);
	}
	if (!strcmp(name, ang_term_name[9])) {
		n = getenv("TOMENET_X11_WID_TERM_9");
		if (n) win_cols = atoi(n);
		n = getenv("TOMENET_X11_HGT_TERM_9");
		if (n) win_lines = atoi(n);
	}

	/* Reset timers just to be sure. */
	td->resize_timer.tv_sec = 0;
	td->resize_timer.tv_usec = 0;

	/* Hack -- Assume full size windows */
	wid = win_cols * td->fnt->wid;
	hgt = win_lines * td->fnt->hgt;
	wid_outer = wid + (2 * DEFAULT_X11_INNER_BORDER_WIDTH);
	hgt_outer = hgt + (2 * DEFAULT_X11_INNER_BORDER_WIDTH);

	/* Create a top-window. */
	MAKE(td->outer, infowin);
	Infowin_set(td->outer);
	Infowin_init_top(topx, topy, wid_outer, hgt_outer, DEFAULT_X11_OUTER_BORDER_WIDTH, Metadpy->fg, Metadpy->bg);
	Infowin_set_mask(StructureNotifyMask | KeyPressMask);
	if (!strcmp(name, ang_term_name[0])) {
		char version[MAX_CHARS];

		sprintf(version, "TomeNET %d.%d.%d%s",
		    VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, CLIENT_VERSION_TAG);
		Infowin_set_name(version);
	} else Infowin_set_name(name);
	Infowin_set_class_hint(name);
	Infowin_set_size_hints(topx, topy, wid_outer, hgt_outer, DEFAULT_X11_INNER_BORDER_WIDTH, td->fnt->wid, td->fnt->hgt, fixed);
	Infowin_map();

	/* Create a sub-window for playing field */
	MAKE(td->inner, infowin);
	Infowin_set(td->inner);
	Infowin_init_std(td->outer, 0, 0, wid, hgt, DEFAULT_X11_INNER_BORDER_WIDTH);
	Infowin_set_mask(ExposureMask);
	Infowin_map();

#ifdef USE_GRAPHICS
	/* No graphics yet */
	td->tiles = NULL;
	td->bgmask = None;
	td->fgmask = None;
 #ifdef GRAPHICS_BG_MASK
	td->bg2mask = None;
	td->tilePreparation2 = None;
 #endif
	td->tilePreparation = None;
 #ifdef TILE_CACHE_SIZE
	if (!disable_tile_cache)
	for (int i = 0; i < TILE_CACHE_SIZE; i++) {
		td->tile_cache[i].tilePreparation = None;
		td->tile_cache[i].c = 0xffffffff;
		td->tile_cache[i].a = 0xff;
  #ifdef GRAPHICS_BG_MASK
		td->tile_cache[i].tilePreparation2 = None;
		td->tile_cache[i].c_back = 0xffffffff;
		td->tile_cache[i].a_back = 0xff;
  #endif
		td->tile_cache[i].is_valid = FALSE;
	}
 #endif
#endif /* USE_GRAPHICS */

	/* Initialize the term (full size) */
	term_init(t, win_cols, win_lines, num);

	/* Use a "soft" cursor */
	t->soft_cursor = TRUE;

	/* Erase with "white space" */
	t->attr_blank = TERM_WHITE;
	t->char_blank = ' ';

	/* Hooks */
	t->xtra_hook = Term_xtra_x11;
	t->curs_hook = Term_curs_x11;
	t->wipe_hook = Term_wipe_x11;
	t->text_hook = Term_text_x11;
	t->nuke_hook = Term_nuke_x11;

#ifdef USE_GRAPHICS
	/* Use graphics */
	if (use_graphics) {
		logprint(format("Termdata graphics init (%d): fwid %d, fhgt %d.\n", index, td->fnt->wid, td->fnt->hgt));

		/* Use resized tiles & masks. */
#ifdef GRAPHICS_BG_MASK
		td->tiles = ResizeImage_2mask(Metadpy->dpy, graphics_image,
				graphics_tile_wid, graphics_tile_hgt, td->fnt->wid, td->fnt->hgt,
				graphics_bgmask, graphics_fgmask, graphics_bg2mask, &(td->bgmask), &(td->fgmask), &(td->bg2mask));
#else
		td->tiles = ResizeImage(Metadpy->dpy, graphics_image,
				graphics_tile_wid, graphics_tile_hgt, td->fnt->wid, td->fnt->hgt,
				graphics_bgmask, graphics_fgmask, &(td->bgmask), &(td->fgmask));
#endif

		/* Initialize preparation pixmap. */
		td->tilePreparation = XCreatePixmap(
				Metadpy->dpy, Metadpy->root,
				td->fnt->wid, td->fnt->hgt, td->tiles->depth);
 #ifdef GRAPHICS_BG_MASK
		td->tilePreparation2 = XCreatePixmap(
				Metadpy->dpy, Metadpy->root,
				td->fnt->wid, td->fnt->hgt, td->tiles->depth);
 #endif

		/* Note: If we want to cache even more graphics for faster drawing, we could initialize 16 copies of the graphics image with all possible mask colours already applied.
		   Memory cost could become "large" quickly though (eg 5MB bitmap -> 80MB). Not a real issue probably. */
 #ifdef TILE_CACHE_SIZE
		if (!disable_tile_cache) {
			for (int i = 0; i < TILE_CACHE_SIZE; i++) {
				td->tile_cache[i].tilePreparation = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
  #ifdef GRAPHICS_BG_MASK
				td->tile_cache[i].tilePreparation2 = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
  #endif
			}
		}
 #endif

		if (td->tiles != NULL && td->tilePreparation != None
 #ifdef GRAPHICS_BG_MASK
		    && td->tilePreparation2 != None
 #endif
		    ) {
			/* Graphics hook */
 #ifdef GRAPHICS_BG_MASK
			if (use_graphics == UG_2MASK) t->pict_hook_2mask = Term_pict_x11_2mask;
 #endif
			t->pict_hook = Term_pict_x11;

			/* Use graphics sometimes */
			t->higher_pict = TRUE;
		}
		else {
			fprintf(stderr, "Couldn't prepare images for terminal %d\n", index);
		}
	}

#endif /* USE_GRAPHICS */

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
 * These colors are overwritten with the generic, OS-independant client_color_map[] in enable_common_colormap_x11()!
 */
static char color_name[CLIENT_PALETTE_SIZE][8] = {
	"#000000",      /* BLACK */
	"#ffffff",      /* WHITE */
	"#9d9d9d",      /* GRAY */
	"#ff8d00",      /* ORANGE */
	"#b70000",      /* RED */
	"#009d44",      /* GREEN */
 #ifndef READABILITY_BLUE
	"#0000ff",      /* BLUE */
 #else
	"#0033ff",      /* BLUE */
 #endif
	"#8d6600",      /* BROWN */
 #ifndef DISTINCT_DARK
	"#747474",      /* DARKGRAY */
 #else
	//"#585858",      /* DARKGRAY */
	"#666666",      /* DARKGRAY */
 #endif
	"#cdcdcd",      /* LIGHTGRAY */
	"#af00ff",      /* PURPLE */
	"#ffff00",      /* YELLOW */
	"#ff3030",      /* PINK */
	"#00ff00",      /* LIGHTGREEN */
	"#00ffff",      /* LIGHTBLUE */
	"#c79d55",      /* LIGHTBROWN */
#ifdef EXTENDED_COLOURS_PALANIM
	/* And clone the above 16 standard colours again here: */
	"#000000",      /* BLACK */
	"#ffffff",      /* WHITE */
	"#9d9d9d",      /* GRAY */
	"#ff8d00",      /* ORANGE */
	"#b70000",      /* RED */
	"#009d44",      /* GREEN */
 #ifndef READABILITY_BLUE
	"#0000ff",      /* BLUE */
 #else
	"#0033ff",      /* BLUE */
 #endif
	"#8d6600",      /* BROWN */
 #ifndef DISTINCT_DARK
	"#747474",      /* DARKGRAY */
 #else
	//"#585858",      /* DARKGRAY */
	"#666666",      /* DARKGRAY */
 #endif
	"#cdcdcd",      /* LIGHTGRAY */
	"#af00ff",      /* PURPLE */
	"#ffff00",      /* YELLOW */
	"#ff3030",      /* PINK */
	"#00ff00",      /* LIGHTGREEN */
	"#00ffff",      /* LIGHTBLUE */
	"#c79d55",      /* LIGHTBROWN */
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
static void enable_common_colormap_x11() {
	int i;
	unsigned long c;
#ifdef EXTENDED_BG_COLOURS
	unsigned long b;
#endif

	for (i = 0; i < CLIENT_PALETTE_SIZE; i++) {
		c = client_color_map[i];

		sprintf(color_name[i], "#%06lx", c & 0xffffffL);
	}

#ifdef EXTENDED_BG_COLOURS
	for (i = 0; i < TERMX_AMT; i++) {
		c = client_ext_color_map[i][0];
		b = client_ext_color_map[i][1];

		sprintf(color_ext_name[i][0], "#%06lx", c & 0xffffffL);
		sprintf(color_ext_name[i][1], "#%06lx", b & 0xffffffL);
	}
#endif
}

void enable_readability_blue_x11(void) {
	/* New colour code */
	client_color_map[6] = 0x0033ff;
#ifdef EXTENDED_COLOURS_PALANIM
	client_color_map[BASE_PALETTE_SIZE + 6] = 0x0033ff;
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
 * Initialization of i-th X11 terminal window.
 */
static errr x11_term_init(int term_id) {
	cptr fnt_name;
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

	/* Check environment for X11 terminal font. */
	fnt_name = getenv(x11_terms_font_env[term_id]);
	/* Check environment for "base" font. */
	if (!fnt_name) fnt_name = getenv("TOMENET_X11_FONT");
	/* Use loaded (from config file) or predefined default font. */
	if (!fnt_name && strlen(term_prefs[term_id].font)) fnt_name = term_prefs[term_id].font;
	/* Paranoia, use the default. */
	if (!fnt_name) fnt_name = x11_terms_font_default[term_id];

	/* Initialize the terminal window, allow resizing, for font changes. */
	err = term_data_init(term_id, x11_terms_term_data[term_id], FALSE, ang_term_name[term_id], fnt_name);
	/* Store created terminal with X11 term data to ang_term array, even if term_data_init failed, but only if there is one. */
	if (Term && term_data_to_term_idx(Term->data) == term_id) ang_term[term_id] = Term;

	if (err) {
		fprintf(stderr, "Error initializing term_data for X11 terminal with index %d\n", term_id);
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
int init_graphics_x11(void) {
	char path[1024];
	char filename[1024];
	int width = 0, height = 0;
	char *data = NULL;
	errr rerr = 0;
	int depth, x, y;
	Visual *visual;

	/* Load graphics file. Quit if file missing or load error. */
	logprint("Initializing graphics.\n");

	/* Check for tiles string & extract tiles width & height. */
	if (2 != sscanf(graphic_tiles, "%dx%d", &graphics_tile_wid, &graphics_tile_hgt)) {
		sprintf(use_graphics_errstr, "Couldn't extract tile dimensions from: %s", graphic_tiles);
		fprintf(stderr, "%s\n", use_graphics_errstr);
 #ifndef GFXERR_FALLBACK
		quit("Graphics load error (X1)");
 #else
		use_graphics = 0;
		use_graphics_err = 101;
		goto gfx_skip;
 #endif
	}

	if (graphics_tile_wid <= 0 || graphics_tile_hgt <= 0) {
		sprintf(use_graphics_errstr, "Invalid tiles dimensions: %dx%d", graphics_tile_wid, graphics_tile_hgt);
		fprintf(stderr, "%s\n", use_graphics_errstr);
 #ifndef GFXERR_FALLBACK
		quit("Graphics load error (X2)");
 #else
		use_graphics = 0;
		use_graphics_err = 102;
		goto gfx_skip;
 #endif
	}

	/* Early-initialize paths, to get access to lib directories right now, we need it for loading the graphics: */
	init_stuff();

	/* Build & allocate the graphics path. */
	path_build(path, 1024, ANGBAND_DIR_XTRA, "graphics");
	ANGBAND_DIR_XTRA_GRAPHICS = string_make(path);

	/* Build the name of the graphics file. */
	path_build(filename, 1024, ANGBAND_DIR_XTRA_GRAPHICS, graphic_tiles);
	strcat(filename, ".bmp");

	/* Load .bmp image. */

	if (0 != (rerr = ReadBMPData(filename, &data, &width, &height))) {
		sprintf(use_graphics_errstr, "Graphics file \"%s\" ", filename);
		switch (rerr) {
		case ReadBMPNoFile:			strcat(use_graphics_errstr, "can not be read."); break;
		case ReadBMPReadErrorOrUnexpectedEOF:	strcat(use_graphics_errstr, "read error or unexpected end."); break;
		case ReadBMPInvalidFile:		strcat(use_graphics_errstr, "has incorrect BMP file format."); break;
		case ReadBMPNoImageData:		strcat(use_graphics_errstr, "contains no image data."); break;
		case ReadBMPUnexpectedEOF:		strcat(use_graphics_errstr, "unexpected end."); break;
		case ReadBMPIllegalBitCount:		strcat(use_graphics_errstr, "has illegal bit count, only 24bit and 32bit images are allowed."); break;
		default: 				strcat(use_graphics_errstr, "unexpected error.");
		}
		fprintf(stderr, "%s\n", use_graphics_errstr);
 #ifndef GFXERR_FALLBACK
		quit("Graphics load error (X3)");
 #else
		use_graphics = 0;
		use_graphics_err = 103;
		goto gfx_skip;
 #endif
	}

	/* Ensure the BMP isn't empty or too small */
	if (width < graphics_tile_wid || height < graphics_tile_hgt) {
		sprintf(use_graphics_errstr, "Invalid image dimensions (width x height): %dx%d", width, height);
		logprint(format("%s\n", use_graphics_errstr));
 #ifndef GFXERR_FALLBACK
		quit("Graphics load error (X4)");
 #else
		use_graphics = 0;
		use_graphics_err = 104;
		goto gfx_skip;
 #endif
	}

	/* Calculate tiles per row. */
	graphics_image_tpr = width / graphics_tile_wid;
	if (graphics_image_tpr <= 0) { /* Paranoia. */
		sprintf(use_graphics_errstr, "Invalid image tiles per row count: %d", graphics_image_tpr);
		fprintf(stderr, "%s\n", use_graphics_errstr);
 #ifndef GFXERR_FALLBACK
		quit("Graphics load error (X5)");
 #else
		use_graphics = 0;
		use_graphics_err = 105;
		goto gfx_skip;
 #endif
	}

	/* Create masks from loaded data */
#ifdef GRAPHICS_BG_MASK
	createMasksFromData_2mask(data, width, height, &graphics_bgmask, &graphics_fgmask, &graphics_bg2mask);
#else
	createMasksFromData(data, width, height, &graphics_bgmask, &graphics_fgmask);
#endif

	/* Store loaded image data in XImage format */
	depth = DefaultDepth(Metadpy->dpy, DefaultScreen(Metadpy->dpy));
	visual = DefaultVisual(Metadpy->dpy, DefaultScreen(Metadpy->dpy));
	graphics_image = XCreateImage(
		Metadpy->dpy, visual, depth, ZPixmap, 0 /*offset*/,
		data, width, height, 32 /*bitmap_pad*/, 0 /*bytes_per_line*/);

	/* Speedup Hack. Don't create and allocate pixel if default depth is 24bit. Is this kosher? */
	if (depth != 24) {
		/* Allocate color for each pixel and rewrite in image */
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				/* Utilize XAllocColor() in create_pixel() to get an 'official' X11 colour for our desired RGB value.
				   On 24-bit(also 32-bit?)-depth visuals this will be identical,
				   so this is mainly for super old/weird hardware that has indexed colours or <8 bits per channel. */
				XPutPixel(graphics_image, x, y, create_pixel(Metadpy->dpy, data[4 * (x + y * width)], data[4 * (x + y * width) + 1], data[4 * (x + y * width) + 2]));
			}
		}
	}

gfx_skip:
	if (!use_graphics) {
		logprint("Disabling graphics and falling back to normal text mode.\n");
		/* Actually also show it as 'off' in =g menu, as in, "desired at config-file level" */
		use_graphics_new = FALSE;
	}

	logprint(format("Graphics initialization complete, use_graphics is %d.\n", use_graphics));
	return(use_graphics);
}
#endif

/*
 * Initialization function for an "X11" module to Angband
 */
errr init_x11(void) {
	int i;
	cptr dpy_name = "";
	char script_path[4096] = { 0 };
	FILE *fp = NULL;

	/* Init the Metadpy if possible */
	if (Metadpy_init_name(dpy_name)) return(-1);

	/* Check if set-font-path.sh script exists and run it - mikaelh */
	if (*path) {
		/* Custom 'lib' directory specified via -P option */
		snprintf(script_path, sizeof(script_path), "%s/xtra/posix_extra_fonts/set-font-path.sh", path);
	} else {
		snprintf(script_path, sizeof(script_path), "lib/xtra/posix_extra_fonts/set-font-path.sh");
	}
	//printf("script_path = %s\n", script_path);
	fp = fopen(script_path, "r");
	if (fp) {
		fclose(fp);
		//printf("Calling external script %s\n", script_path);
		if (system(script_path) != 0) {
			fprintf(stderr, "Failed to run external script %s\n", script_path);
		}
	}

	/* set OS-specific resize_main_window() hook */
	resize_main_window = resize_main_window_x11;

	enable_common_colormap_x11();

	/* Prepare color "xor" (for cursor) */
	MAKE(xor, infoclr);
	Infoclr_set (xor);
	Infoclr_init_ccn ("fg", "bg", "xor", 0);

	/* Prepare the colors (including "black") */
	for (i = 0; i < CLIENT_PALETTE_SIZE; ++i) {
		cptr cname = color_name[0];

		MAKE(clr[i], infoclr);
		Infoclr_set(clr[i]);
		if (Metadpy->color) cname = color_name[i];
		else if (i) cname = color_name[1];
		Infoclr_init_ccn (cname, "bg", "cpy", 0);
	}

#ifdef EXTENDED_BG_COLOURS
	/* Prepare the extended background-using colors */
	for (i = 0; i < TERMX_AMT; ++i) {
		cptr cname = color_name[0], cname2 = color_name[0];

		MAKE(clr[CLIENT_PALETTE_SIZE + i], infoclr);
		Infoclr_set(clr[CLIENT_PALETTE_SIZE + i]);
		if (Metadpy->color) {
			cname = color_ext_name[i][0];
			cname2 = color_ext_name[i][1];
		}
		Infoclr_init_ccn (cname, cname2, "cpy", 0);
	}
#endif

#ifdef USE_GRAPHICS
	if (use_graphics) (void)init_graphics_x11();
#endif /* USE_GRAPHICS */

	/* Initialize each term */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		/* Main window is always visible, all other depend on configuration. */
		if (i == 0 || term_prefs[i].visible) {
			if (x11_term_init(i) != 0) {
				fprintf(stderr, "Error initializing X11 terminal window with index %d\n", i);
				/* Can't run without main screen. */
				if (i == 0) return(1);
			}
		}
	}

	/* Activate the "Angband" main window screen. */
	Term_activate(&term_main.t);

	/* Raise the "Angband" main window. */
	Infowin_set(term_main.outer);
	Infowin_raise();

	/* Success */
	return(0);
}


#if 0
/* Turn off num-lock if it's on */
void turn_off_numlock_X11(void) {
	Display* disp = XOpenDisplay(NULL);

	if (disp == NULL) return; /* Error */

	XTestFakeKeyEvent(disp, XKeysymToKeycode(disp, XK_Num_Lock), True, 0);
	XTestFakeKeyEvent(disp, XKeysymToKeycode(disp, XK_Num_Lock), False, 0);
	XFlush(disp);
	XCloseDisplay(disp);
}
#endif


#if 1 /* CHANGE_FONTS_X11 */
/* EXPERIMENTAL // allow user to change main window font live - C. Blue
   So far only uses 1 parm ('s') to switch between hardcoded choices:
   -1 - cycle
    0 - normal
    1 - big
    2 - bigger
    3 - huge */
/* only offer 2 cycling stages? */
#define REDUCED_FONT_CHOICE
static void term_force_font(int term_idx, cptr fnt_name);
void change_font(int s) {
	/* use main window font for measuring */
	char tmp[128] = "";

	if (term_main.fnt->name) strcpy(tmp, term_main.fnt->name);
	else strcpy(tmp, DEFAULT_X11_FONT);

	/* cycle? */
	if (s == -1) {
#ifdef REDUCED_FONT_CHOICE
		if (strstr(tmp, "8x13")) s = 1;
		else if (strstr(tmp, "9x15")) s = 0;
#else
		if (strstr(tmp, "8x13") || strstr(tmp, "lucidasanstypewriter-8")) s = 1;
		else if (strstr(tmp, "lucidasanstypewriter-10")) s = 2;
#endif
		else if (strstr(tmp, "lucidasanstypewriter-12")) s = 3;
		else if (strstr(tmp, "lucidasanstypewriter-18")) s = 0;
	}

	/* Force the font */
	switch (s) {
	case 0:
		/* change main window font */
#ifdef REDUCED_FONT_CHOICE
		term_force_font(0, "8x13");
#else
		term_force_font(0, "lucidasanstypewriter-8");
#endif
		/* Change sub windows too */
		term_force_font(1, "8x13"); //msg
		term_force_font(2, "8x13"); //inv
		term_force_font(3, "5x8"); //char
		term_force_font(4, "6x10"); //chat
		term_force_font(5, "6x10"); //eq (5x8)
		term_force_font(6, "5x8");
		term_force_font(7, "5x8");
		break;
	case 1:
		/* change main window font */
#ifdef REDUCED_FONT_CHOICE
		term_force_font(0, "9x15");//was 10x14x
#else
		term_force_font(0, "lucidasanstypewriter-10");
#endif
		/* Change sub windows too */
		term_force_font(1, "9x15");
		term_force_font(2, "9x15");
		term_force_font(3, "6x10");
		term_force_font(4, "8x13");
		term_force_font(5, "6x10");
		term_force_font(6, "6x10");
		term_force_font(7, "6x10");
		break;
	case 2:
		/* change main window font */
		term_force_font(0, "lucidasanstypewriter-12");
		/* Change sub windows too */
		term_force_font(1, "9x15");
		term_force_font(2, "9x15");
		term_force_font(3, "8x13");
		term_force_font(4, "9x15");
		term_force_font(5, "8x13");
		term_force_font(6, "8x13");
		term_force_font(7, "8x13");
		break;
	case 3:
		/* change main window font */
		term_force_font(0, "lucidasanstypewriter-18");
		/* Change sub windows too */
		term_force_font(1, "lucidasanstypewriter-12");
		term_force_font(2, "lucidasanstypewriter-12");
		term_force_font(3, "9x15");
		term_force_font(4, "lucidasanstypewriter-12");
		term_force_font(5, "9x15");
		term_force_font(6, "9x15");
		term_force_font(7, "9x15");
		break;
	}
}
static void term_force_font(int term_idx, cptr fnt_name) {
	term_data *td = term_idx_to_term_data(term_idx);
	int cols, rows, wid_outer, hgt_outer;

	/* non-visible window has no fnt-> .. */
	if (!term_get_visibility(term_idx)) return;

	/* special hack: this window was invisible, but we just toggled
	   it to become visible on next client start. - C. Blue */
	if (!td->fnt) return;

	/* Determine "proper" number of rows/cols */
	cols = ((td->outer->w - (2 * td->inner->b)) / td->fnt->wid);
	rows = ((td->outer->h - (2 * td->inner->b)) / td->fnt->hgt);

	/* Create and initialize font. */
	infofnt *new_font;
	MAKE(new_font, infofnt);
	infofnt *old_infofnt = Infofnt;
	Infofnt_set(new_font);
	if (Infofnt_init_data(fnt_name)) {
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
	if (td->fnt->info) XFreeFont(Metadpy->dpy, td->fnt->info);
	FREE(td->fnt, infofnt);
	td->fnt = new_font;

	/* Desired size of "outer" window */
	wid_outer = (cols * td->fnt->wid) + (2 * td->inner->b);
	hgt_outer = (rows * td->fnt->hgt) + (2 * td->inner->b);

	/* Resize the windows if any "change" is needed */
	if ((td->outer->w != wid_outer) || (td->outer->h != hgt_outer)) {
		/* First set size hints again. */
		infowin *iwin = Infowin;
		Infowin_set(td->outer);
		Infowin_set_size_hints(Infowin->x, Infowin->y, wid_outer, hgt_outer, td->inner->b, td->fnt->wid, td->fnt->hgt, FALSE);
		Infowin_set(iwin);

		resize_window_x11(term_idx, cols, rows);

#ifdef USE_GRAPHICS
		/* Resize graphic tiles if needed too. */
		if (use_graphics) {
			/* Free old tiles & masks */
			free_graphics(td);

			/* If window was resized, grapics tiles need to be resized too. */
 #ifdef GRAPHICS_BG_MASK
			td->tiles = ResizeImage_2mask(Metadpy->dpy, graphics_image,
					graphics_tile_wid, graphics_tile_hgt, td->fnt->wid, td->fnt->hgt,
					graphics_bgmask, graphics_fgmask, graphics_bg2mask, &(td->bgmask), &(td->fgmask), &(td->bg2mask));
 #else
			td->tiles = ResizeImage(Metadpy->dpy, graphics_image,
					graphics_tile_wid, graphics_tile_hgt, td->fnt->wid, td->fnt->hgt,
					graphics_bgmask, graphics_fgmask, &(td->bgmask), &(td->fgmask));
 #endif

			/* Reinitialize preparation pixmap with new size. */
			td->tilePreparation = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
 #ifdef GRAPHICS_BG_MASK
			td->tilePreparation2 = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
 #endif

 #ifdef TILE_CACHE_SIZE
			if (!disable_tile_cache)
			for (int i = 0; i < TILE_CACHE_SIZE; i++) {
				td->tile_cache[i].tilePreparation = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
  #ifdef GRAPHICS_BG_MASK
				td->tile_cache[i].tilePreparation2 = XCreatePixmap(
					Metadpy->dpy, Metadpy->root,
					td->fnt->wid, td->fnt->hgt, td->tiles->depth);
  #endif
			}
 #endif

			if (td->tiles == NULL || td->tilePreparation == None
 #ifdef GRAPHICS_BG_MASK
			    || td->tilePreparation2 == None
 #endif
			    ) {
				quit_fmt("Couldn't prepare images after font resize in terminal %d\n", term_idx);
			}
		}
#endif
	}
	XFlush(Metadpy->dpy);

	/* Reload custom font prefs on main screen font change */
	if (td == &term_main) handle_process_font_file();
}
#endif

/* Used for checking window position on mapping and saving window positions on quitting.
   Returns ret_x, ret_y containing window coordinates relative to root display window, corrected for decorations. */
void terminal_window_real_coords_x11(int term_idx, int *ret_x, int *ret_y) {
	term_data *td = term_idx_to_term_data(term_idx);
	infowin *iwin;
	Window xid, tmp_win;
	unsigned int wu, hu, bu, du;
	Atom property;
	Atom type_return;
	int format_return;
	unsigned long nitems_return;
	unsigned long bytes_after_return;
	unsigned char *data;
	int x_rel, y_rel;
	char got_frame_extents = FALSE;

	/* non-visible window has no window info .. */
	if (!term_get_visibility(term_idx)) {
		*ret_x = *ret_y = 0;
		return;
	}


	/* special hack: this window was invisible, but we just toggled
	   it to become visible on next client start. - C. Blue */
	if (!td->fnt) *ret_x = *ret_y = 0;

	iwin = td->outer;
	Infowin_set(iwin);
	xid = iwin->win;

	property = XInternAtom(Metadpy->dpy, "_NET_FRAME_EXTENTS", True);

	/* Try to use _NET_FRAME_EXTENTS to get the window borders */
	if (property != None && XGetWindowProperty(Metadpy->dpy, xid, property,
	    0, LONG_MAX, False, XA_CARDINAL, &type_return, &format_return,
	    &nitems_return, &bytes_after_return, &data) == Success) {
		if ((type_return == XA_CARDINAL) && (format_return == 32) && (nitems_return == 4) && (data)) {
			unsigned long *ldata = (unsigned long *)data;

			got_frame_extents = TRUE;
			/* _NET_FRAME_EXTENTS format is left, right, top, bottom */
			x_rel = ldata[0];
			y_rel = ldata[2];
		}

		if (data) XFree(data);
	}

	if (!got_frame_extents) {
		/* Check For Error XXX Extract some ACTUAL data from 'xid' */
		if (XGetGeometry(Metadpy->dpy, xid, &tmp_win, &x_rel, &y_rel, &wu, &hu, &bu, &du)) {
		} else {
			x_rel = 0;
			y_rel = 0;
		}
	}

	XTranslateCoordinates(Metadpy->dpy, xid, Metadpy->root, 0, 0, ret_x, ret_y, &tmp_win);

	/* correct window position to account for X11 border/title bar sizes */
	*ret_x -= x_rel;
	*ret_y -= y_rel;
}

/* Resizes the graphics terminal window described by 'td' to dimensions given in 'cols', 'rows' inputs.
 * Stops the resize timer and validates input.
 * Resizes the outer and inner X11 window if current dimensions don't match the validated dimensions.
 * The terminal stored in 'td->t' is resized to desired size if needed.
 * When the window is the main window, update the screen globals, handle bigmap and notify server if in game.
 */
void resize_window_x11(int term_idx, int cols, int rows) {
	bool rounding_down;
	term_data *td;
	int wid_inner, hgt_inner, wid_outer, hgt_outer;

	/* The 'term_idx_to_term_data()' returns '&term_main' if 'term_idx' is out of bounds and it is not desired to resize screen terminal window in that case, so validate before. */
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;
	td = term_idx_to_term_data(term_idx);

	/* Clear timer. */
	if (td->resize_timer.tv_sec > 0 || td->resize_timer.tv_usec > 0) {
		td->resize_timer.tv_sec = 0;
		td->resize_timer.tv_usec = 0;
	}

	/* Validate input dimensions. */
	/* Our 'term_data' indexes in 'term_idx' are the same as 'ang_term' indexes so it's safe to use 'validate_term_dimensions()'. */
	rounding_down = validate_term_dimensions(term_idx, &cols, &rows);
	/* Are we actually enlarging the window? */
	if (td == &term_main && rounding_down && screen_hgt == SCREEN_HGT) rows = MAX_SCREEN_HGT + SCREEN_PAD_Y;

	/* Calculate dimensions in pixels. */
	wid_inner = cols * td->fnt->wid;
	hgt_inner = rows * td->fnt->hgt;
	wid_outer = wid_inner + (2 * td->inner->b);
	hgt_outer = hgt_inner + (2 * td->inner->b);

	/* Save current Infowin. */
	infowin *iwin = Infowin;

	/* Resize the outer window if dimensions differ. */
	Infowin_set(td->outer);
	if ((Infowin->w != wid_outer) || (Infowin->h != hgt_outer))
		Infowin_resize(wid_outer, hgt_outer);

	/* Resize the inner window if dimensions differ. */
	Infowin_set(td->inner);
	if (Infowin->w != wid_inner || Infowin->h != hgt_inner)
		Infowin_resize(wid_inner, hgt_inner);

	/* Restore saved Infowin. */
	Infowin_set(iwin);

	/* Save current activated Term. */
	term *t = Term;

	/* Make the changes go live (triggers on next c_message_add() call) */
	/* No need to check if dimensions differ, Term_resize handles it. */
	Term_activate(&td->t);
	Term_resize(cols, rows);

	/* Restore saved term. */
	Term_activate(t);

	if (td == &term_main) {
		/* Main screen is special. Update the screen_wid/hgt globals if needed and notify server about it if in game. */

		int new_screen_cols = cols - SCREEN_PAD_X;
		int new_screen_rows = rows - SCREEN_PAD_Y;

		/* avoid bottom line of garbage left from big_screen when shrinking to normal screen.. oO */
		if (new_screen_rows < screen_hgt) clear_from(SCREEN_HGT + SCREEN_PAD_Y - 1);

		if (screen_wid != new_screen_cols || screen_hgt != new_screen_rows) {
#if 1 /* actually since we already do proper calcs for 'rows = ' above with rounding_down and also in validate_screen_dimensions(), this whole if-block here is 100% redundant! but w/e */
			/* allow only 22 and 44 map lines (normal vs big_map) */
			if (new_screen_rows <= SCREEN_HGT)
				new_screen_rows = SCREEN_HGT;
			else if (new_screen_rows < MAX_SCREEN_HGT) { //can currently not happen, because validate_screen_dimensions() will cut it down; we use rounding_down as workaround!
				/* are we enlarging or shrinking the window? */
				if (screen_hgt < new_screen_rows) new_screen_rows = MAX_SCREEN_HGT;
				else new_screen_rows = SCREEN_HGT;
			} else new_screen_rows = MAX_SCREEN_HGT;
#endif

			screen_wid = new_screen_cols;
			screen_hgt = new_screen_rows;

			if (in_game) {
				/* Switch big_map mode . */
#ifndef GLOBAL_BIG_MAP
				if (Client_setup.options[CO_BIGMAP] && rows == DEFAULT_TERM_HGT) {
					/* Turn off big_map. */
					c_cfg.big_map = FALSE;
					Client_setup.options[CO_BIGMAP] = FALSE;
				} else if (!Client_setup.options[CO_BIGMAP] && rows != DEFAULT_TERM_HGT) {
					/* Turn on big_map. */
					c_cfg.big_map = TRUE;
					Client_setup.options[CO_BIGMAP] = TRUE;
				}
#else
				if (global_c_cfg_big_map && rows == DEFAULT_TERM_HGT) {
					/* Turn off big_map. */
					global_c_cfg_big_map = FALSE;
				} else if (!global_c_cfg_big_map && rows != DEFAULT_TERM_HGT) {
					/* Turn on big_map. */
					global_c_cfg_big_map = TRUE;
				}
#endif
				/* Notify server and ask for a redraw. */
				Send_screen_dimensions();
			}
		}
	}

	/* Ask for a redraw. */
	if (in_game) cmd_redraw();
}

/* Resizes main terminal window to dimensions in input. */
/* Used for OS-specific resize_main_window() hook. */
void resize_main_window_x11(int cols, int rows) {
	resize_window_x11(0, cols, rows);
}

bool ask_for_bigmap(void) {
	return(ask_for_bigmap_generic());
}

const char* get_font_name(int term_idx) {
	term_data *td = term_idx_to_term_data(term_idx);

	if (td->fnt) return(td->fnt->name);
	if (strlen(term_prefs[term_idx].font)) return(term_prefs[term_idx].font);
	return(x11_terms_font_default[term_idx]);
}

void set_font_name(int term_idx, char* fnt) {
	term_data *td;

	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) {
		fprintf(stderr, "Terminal index %d is out of bounds for set_font_name\n", term_idx);
		return;
	}

	if (!term_get_visibility(term_idx)) {
		/* Terminal is not visible, Do nothing, just change the font name in preferences. */
		if (strcmp(term_prefs[term_idx].font, fnt) != 0) {
			strncpy(term_prefs[term_idx].font, fnt, sizeof(term_prefs[term_idx].font));
			term_prefs[term_idx].font[sizeof(term_prefs[term_idx].font) - 1] = '\0';
		}
		return;
	}

	term_force_font(term_idx, fnt);

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
	errr err = x11_term_init(term_idx);
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
}

/* Returns true if terminal window specified by term_idx is currently visible. */
bool term_get_visibility(int term_idx) {
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return(false);

	/* Only windows initialized in ang_term array are currently visible. */
	return((bool)ang_term[term_idx]);
}

/* automatically store name+password to ini file if we're a new player? */
void store_crecedentials(void) {
	write_mangrc(TRUE, TRUE, FALSE);
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
/* Palette animation - 2018 *testing* */
void animate_palette(void) {
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
			sprintf(color_name[i], "#%06lx", code & 0xffffffL);
			c_message_add(format("changed [%d] %d -> %d (%d,%d,%d)", i, color_name[i], code, rv, gv, bv));
		}
	}

	/* Activate the palette */
	for (i = 0; i < BASE_PALETTE_SIZE; ++i) {
		cptr cname = color_name[0];

		XFreeGC(Metadpy->dpy, clr[i]->gc);
		//MAKE(clr[i], infoclr);
		Infoclr_set (clr[i]);
#if 0 /* no colours on this display? */
		if (Metadpy->color) cname = color_name[i];
		else if (i) cname = color_name[1];
#else
		cname = color_name[i];
#endif
		Infoclr_init_ccn (cname, "bg", "cpy", 0);
	}

	/* Refresh aka redraw windows with new colour */
	old_td = (term_data*)(Term->data);
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
 #if 0 /* todo: fix/implement live-updating of 'bg' colour (tethered to colour #0) */
  #ifdef CUSTOMIZE_COLOURS_FREELY
		/* Handle change of colour #0, which also serves as the designated bg colour now */
		//if (!c) {
		if (TRUE) {
			Pixell pixel = create_pixel(Metadpy->dpy, (client_color_map[0] & 0xFF0000) >> 16, (client_color_map[0] & 0x00FF00) >> 8, client_color_map[0] & 0x0000FF);
			Metadpy->bg = pixel;
			Metadpy_update(1, 1, 1);

			int i;
			Infoclr_init_ccn ("fg", "bg", "xor", 0);

			/* Prepare the colors (including "black") */
			for (i = 0; i < CLIENT_PALETTE_SIZE; ++i) {
				cptr cname = color_name[0];

			MAKE(clr[i], infoclr);
				Infoclr_set(clr[i]);
				if (Metadpy->color) cname = color_name[i];
				else if (i) cname = color_name[1];
				Infoclr_init_ccn (cname, "bg", "cpy", 0);
			}

   #ifdef EXTENDED_BG_COLOURS
			/* Prepare the extended background-using colors */
			for (i = 0; i < TERMX_AMT; ++i) {
				cptr cname = color_name[0], cname2 = color_name[0];

				MAKE(clr[CLIENT_PALETTE_SIZE + i], infoclr);
				Infoclr_set(clr[CLIENT_PALETTE_SIZE + i]);
				if (Metadpy->color) {
					cname = color_ext_name[i][0];
					cname2 = color_ext_name[i][1];
				}
				Infoclr_init_ccn (cname, cname2, "cpy", 0);
			}
   #endif
		}
  #endif
 #endif

		/* Refresh aka redraw the main window with new colour */
		if (!term_get_visibility(0)) return;
		if (term_prefs[0].x == -32000 || term_prefs[0].y == -32000) return;
		Term_activate(&term_idx_to_term_data(0)->t);
		/* Invalidate cache to ensure redrawal doesn't get cancelled by tile-caching */
 #if defined(USE_GRAPHICS) && defined(TILE_CACHE_SIZE)
  #if !defined(TILE_CACHE_NEWPAL) && !defined(TILE_CACHE_FGBG)
		/* Last resort: Invalidate whole cache as we didn't keep track of which colours' palettes have been modified. */
		invalidate_graphics_cache_x11(term_idx_to_term_data(0), -1);
  #endif
 #endif
//WiP, not functional		if (screen_icky) Term_switch_fully(0);
		if (c_cfg.gfx_palanim_repaint || (c_cfg.gfx_hack_repaint && !gfx_palanim_repaint_hack))
			Term_repaint(SCREEN_PAD_LEFT, SCREEN_PAD_TOP, screen_wid, screen_hgt); //flicker-free redraw - C. Blue
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
	/* Testing // For extended-bg colours, for now just animate the background part */
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
	XFreeGC(Metadpy->dpy, clr[c]->gc);
	//MAKE(clr[c], infoclr);
	Infoclr_set(clr[c]);

#if 0 /* no colours on this display? */
	if (Metadpy->color) cname = color_name[c];
	else if (c) cname = color_name[1];
#else
 #ifndef EXTENDED_BG_COLOURS
	/* Foreground colour */
	cname = color_name[c];
 #else
	/* For extended colours actually use background colour instead, as this interests us most atm */
	if (c >= CLIENT_PALETTE_SIZE) /* TERMX_.. */
		cname = color_ext_name[c - CLIENT_PALETTE_SIZE][1];
	/* Foreground colour */
	else cname = color_name[c];
 #endif
#endif

#ifdef EXTENDED_BG_COLOURS
	/* Just for testing for now.. */
	if (c >= CLIENT_PALETTE_SIZE) { /* TERMX_.. */
		/* Actually animate the 'bg' colour instead of the 'fg' colour (testing purpose) */
		Infoclr_init_ccn(color_ext_name[c - CLIENT_PALETTE_SIZE][0], cname, "cpy", 0);
	} else
#endif
	Infoclr_init_ccn(cname, "bg", "cpy", 0);

#ifndef PALANIM_OPTIMIZED
	/* Refresh aka redraw the main window with new colour */
	if (!term_get_visibility(0)) return;
	if (term_prefs[0].x == -32000 || term_prefs[0].y == -32000) return;
	Term_activate(&term_idx_to_term_data(0)->t);
 #if defined(USE_GRAPHICS) && defined(TILE_CACHE_SIZE)
  #ifdef TILE_CACHE_NEWPAL
	/* Invalidate cache using this particular colour to ensure redrawal doesn't get cancelled by tile-caching */
	invalidate_graphics_cache_x11(term_idx_to_term_data(0), c);
  #elif !defined(TILE_CACHE_FGBG)
	/* Last resort: Invalidate whole cache as we didn't keep track of which colours' palettes have been modified. */
	invalidate_graphics_cache_x11(term_idx_to_term_data(0), -1);
  #endif
 #endif
	Term_xtra(TERM_XTRA_FRESH, 0);
	Term_activate(&old_td->t);
#else
 #if defined(USE_GRAPHICS) && defined(TILE_CACHE_SIZE) && defined(TILE_CACHE_NEWPAL)
	/* Invalidate cache using this particular colour to ensure redrawal doesn't get cancelled by tile-caching */
	invalidate_graphics_cache_x11(term_idx_to_term_data(0), c);
 #endif
#endif
}
/* Gets R/G/B values from 0..255 for a specific terminal palette entry (not for a TERM_ colour). */
void get_palette(byte c, byte *r, byte *g, byte *b) {
	u32b cref = clr[c]->fg;

	*r = (cref & 0xFF0000) >> 16;
	*g = (cref & 0x00FF00) >> 8;
	*b = (cref & 0x0000FF);
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

/* Get list of available misc fonts, e.g. "5x8", "6x9", "6x13" or "6x13bold". */
int get_misc_fonts(char *output_list, int max_misc_fonts, int max_font_name_length, int max_fonts) {
#ifdef REGEX_SEARCH
	regex_t re;
	char **list;
	int fonts_found = 0, fonts_match = 0, i, j;
	bool is_duplicate;
	int status = -999;

	char tmp[1024];
	FILE *fff;


	path_build(tmp, 1024, ANGBAND_DIR_XTRA, "fonts-x11-menuscan.txt");
	fff = fopen(tmp, "r");
	if (fff) {
		while (fgets(tmp, 256, fff)) {
			if (strncmp(tmp, "REGEXP=", 7)) continue;
			tmp[strlen(tmp) - 1] = 0; //remove trailing \n
			status = regcomp(&re, tmp + 7, REG_EXTENDED | REG_NOSUB | REG_ICASE);
			break;
		}
		fclose(fff);
	}
	if (status == -999) status = regcomp(&re, "^[0-9]+x[0-9]+[a-z]?[a-z]?(bold)?$", REG_EXTENDED | REG_NOSUB | REG_ICASE);

	if (status != 0) {
		fprintf(stderr, "regcomp returned %d\n", status);
		return(0);
	}

	/* Get list of all fonts with 'x' in the name */
	list = XListFonts(Metadpy->dpy, "*x*", 16 * 1024, &fonts_found);
	if (!list) {
		regfree(&re);
		return(0);
	}
	for (i = 0; i < fonts_found && fonts_match < max_misc_fonts; i++) {
		status = regexec(&re, list[i], 0, NULL, 0);
		if (status) continue;
		if (strlen(list[i]) >= max_font_name_length) continue;

		is_duplicate = FALSE;
		for (j = i - 1; j >= 0; j--) {
			if (strcmp(list[i], list[j]) == 0) {
				is_duplicate = TRUE;
				break;
			}
		}
		if (!is_duplicate) {
			strcpy(&output_list[fonts_match * max_font_name_length], list[i]);
			fonts_match++;
			if (fonts_match == max_misc_fonts) c_msg_format("Warning: Number of (misc) fonts exceeds max of %d. Ignoring the rest.", max_fonts);
		}
	}
	regfree(&re);
	XFreeFontNames(list);

	/* done */
	return(fonts_match);

#else /* We try to utilize 'grep' instead! */

	char regex[MAX_CHARS_WIDE] = { 0 };
	char **list;
	int fonts_found = 0, fonts_match = 0, i, j;
	bool is_duplicate;

	char tmp[1024];
	FILE *fff;


	if (system("which grep")) {
		c_msg_print("\377yCannot parse system fonts as neither REGEX_SEARCH is defined nor 'grep' command");
		c_msg_print("\377y is available. You can install package 'grep' (scanning is very slow though).");
		return(0); /* not found */
	} else c_msg_print("\377yREGEX_SEARCH not defined, falling back to 'grep' usage. Please wait, it's slow.");
	Term_fresh();

	path_build(tmp, 1024, ANGBAND_DIR_XTRA, "fonts-x11-menuscan.txt");
	fff = fopen(tmp, "r");
	if (fff) {
		while (fgets(tmp, 256, fff)) {
			if (strncmp(tmp, "REGEXP=", 7)) continue;
			tmp[strlen(tmp) - 1] = 0; //remove trailing \n
			strcpy(regex, tmp + 7);
			break;
		}
		fclose(fff);
	}
	if (!regex[0]) strcpy(regex, "^[0-9]+x[0-9]+[a-z]?[a-z]?(bold)?$");

	/* Get list of all fonts with 'x' in the name */
	list = XListFonts(Metadpy->dpy, "*x*", 16 * 1024, &fonts_found);
	if (!list) return(0);
	for (i = 0; i < fonts_found && fonts_match < max_misc_fonts; i++) {
		system(format("echo '%s' | grep -E '%s' > /tmp/tomenet-grep.tmp", list[i], regex)); // -E: REG_EXTENDED; -P (perl) would also work
		fff = fopen("/tmp/tomenet-grep.tmp", "r");
		if (fff) {
			if (!fgets(tmp, 256, fff)) {
				fclose(fff);
				continue;
			}
			if (!tmp[0] || tmp[0] == '\n') {
				fclose(fff);
				continue;
			}
			fclose(fff);
			// font name is accepted
		} else return(0);

		if (strlen(list[i]) >= max_font_name_length) continue;

		is_duplicate = FALSE;
		for (j = i - 1; j >= 0; j--) {
			if (strcmp(list[i], list[j]) == 0) {
				is_duplicate = TRUE;
				break;
			}
		}
		if (!is_duplicate) {
			strcpy(&output_list[fonts_match * max_font_name_length], list[i]);
			fonts_match++;
			if (fonts_match == max_misc_fonts) c_msg_format("Warning: Number of (misc) fonts exceeds max of %d. Ignoring the rest.", max_fonts);
		}
	}
	remove("/tmp/tomenet-grep.tmp");
	XFreeFontNames(list);

	/* done */
	return(fonts_match);
#endif
}

void set_window_title_x11(int term_idx, cptr title) {
	term_data *td;

	/* The 'term_idx_to_term_data()' returns '&term_main' if 'term_idx' is out of bounds and it is not desired to resize screen terminal window in that case, so validate before. */
	if (term_idx < 0 || term_idx >= ANGBAND_TERM_MAX) return;

	/* Trying to change title in this state causes a crash */
	if (!term_get_visibility(term_idx)) return;
	if (term_prefs[term_idx].x == -32000 || term_prefs[term_idx].y == -32000) return;

	td = term_idx_to_term_data(term_idx);

	/* Save current Infowin. */
	infowin *iwin = Infowin;

	Infowin_set(td->outer);
	Infowin_set_name(ang_term_name[term_idx]);

	/* Restore saved Infowin. */
	Infowin_set(iwin);
}

#endif // USE_X11
