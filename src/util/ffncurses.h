/** ncurses wrapper
2026, Simon Zolin */

#ifdef _WIN32
#undef MOUSE_MOVED
#include <curses.h>
#else
#include <locale.h>
#include <ncurses.h>
#endif
#undef clear

#define ffncurses_height()  getmaxy(stdscr)
#define ffncurses_width()  getmaxx(stdscr)

static inline int imin(int a, int b) { return (a < b) ? a : b; }
static inline int imax(int a, int b) { return (a > b) ? a : b; }

struct ffncurses_rect {
	uint h, w, y, x;
};

static inline struct ffncurses_rect ffncurses_auto_center(uint scale_pct)
{
	unsigned hmax = getmaxy(stdscr)
		, wmax = getmaxx(stdscr);
	struct ffncurses_rect r = {
		.h = imin(2 + hmax * scale_pct / 100, hmax),
		.w = imin(4 + wmax * scale_pct / 100, wmax),
		.y = imax((hmax - r.h) / 2, 0),
		.x = imax((wmax - r.w) / 2, 0),
	};
	return r;
}

/** Get Y/X coordinates for center window positioning */
static inline void ffncurses_center(unsigned h, unsigned w, unsigned *y, unsigned *x)
{
	*y = imax((getmaxy(stdscr) - h) / 2, 0);
	*x = imax((getmaxx(stdscr) - w) / 2, 0);
}

struct ffncurses_conf {
	char colors[10][2];
};

/** Configure a color pair for later use with ffncurses_print_attr().
To indicate the last color use:
	ffncurses_color(..., -1, -1);
i: Color pair index (1-based)
fg: Foreground color constant (e.g., COLOR_MAGENTA) or -1 for default
bg: Background color constant or -1 for default */
static inline void ffncurses_color(struct ffncurses_conf *c, uint i, int fg, int bg) {
	i--;
	FF_ASSERT(i < FF_COUNT(c->colors));
	c->colors[i][0] = fg;
	c->colors[i][1] = bg;
}

struct ffncurses_wnd {
	WINDOW *wnd;
	unsigned ch, cw;
	unsigned modified :1;
	unsigned popup :1;
};

/** Initialize ncurses.
Set up UTF-8 and color support, hide cursor, initialize color pairs. */
static inline void ffncurses_init(struct ffncurses_wnd *w, struct ffncurses_conf *c)
{
#ifndef _WIN32
	setlocale(LC_ALL, "");
#endif
	initscr();
	curs_set(0);
	start_color();
	use_default_colors();
	refresh();

	for (uint i = 0;  c->colors[i][0] != -1 || c->colors[i][1] != -1;  i++) {
		init_pair(i + 1, c->colors[i][0], c->colors[i][1]);
	}

	w->wnd = stdscr;
	w->ch = getmaxy(stdscr);
	w->cw = getmaxx(stdscr);
}

/** Terminate ncurses and restore the terminal to its normal state. */
static inline void ffncurses_end()
{
	endwin();
}

/** Clear a single line.
y: Row */
static inline void ffncurses_line_clear(struct ffncurses_wnd *w, unsigned y)
{
	unsigned x = (w->popup) ? 1 : 0;
	mvwhline(w->wnd, y, x, ' ', getmaxx(w->wnd) - x*2);
	w->modified = 1;
}

/** Print formatted text.
y, x: Row and column coordinates
...: printf-style format string and arguments */
#define ffncurses_printf(w, y, x, ...) \
do { \
	mvwprintw((w)->wnd, y, x, ##__VA_ARGS__); \
	(w)->modified = 1; \
} while (0)

/** Print text with attribute and color styling.
y, x: Row and column coordinates
attr: Attribute flags (e.g., A_BOLD) or 0 for none
color_id: Color pair index (1-based) or 0 for default color */
static inline void ffncurses_printn_attr(struct ffncurses_wnd *w, int y, int x, char *text, unsigned len, unsigned attr, unsigned color_id)
{
	if (attr)
		wattron(w->wnd, attr);
	if (color_id)
		wattron(w->wnd, COLOR_PAIR(color_id));

	if (len >= w->cw) {
		len = w->cw;
		text[len - 3] = text[len - 2] = text[len - 1] = '.'; // "[abcd]ef" -> "a..."
	}
	mvwaddnstr(w->wnd, y, x, text, len);

	if (color_id)
		wattroff(w->wnd, COLOR_PAIR(color_id));
	if (attr)
		wattroff(w->wnd, attr);
	w->modified = 1;
}

/** Print text line. */
static inline void ffncurses_println_attr(struct ffncurses_wnd *w, int y, int x, char *text, unsigned len, unsigned attr, unsigned color_id)
{
	ffncurses_line_clear(w, y);
	ffncurses_printn_attr(w, y, x, text, len, attr, color_id);
}


/** Redraw the window. */
static inline void ffncurses_redraw(struct ffncurses_wnd *w)
{
	wrefresh(w->wnd);
}

static inline void ffncurses_update(struct ffncurses_wnd *w)
{
	if (w->modified) {
		w->modified = 0;
		ffncurses_redraw(w);
	}
}

/** Create a bordered popup window.
h: Height
w: Width
y, x: Top-left corner coordinates */
static inline void ffncurses_popup(struct ffncurses_wnd *w, unsigned h, unsigned width, unsigned y, unsigned x, char *title, unsigned len, unsigned color_id)
{
	w->wnd = newwin(h, width, y, x);
	w->ch = h - 2;
	w->cw = width - 2;
	w->popup = 1;
	box(w->wnd, 0, 0);
	ffncurses_printn_attr(w, 0, 1, title, len, A_BOLD, color_id);
}

static inline void ffncurses_popup_del(struct ffncurses_wnd *w)
{
	delwin(w->wnd);
	w->wnd = NULL;
	w->modified = 0;

	redrawwin(stdscr);
	wrefresh(stdscr);
}
