/** ncurses wrapper
2026, Simon Zolin */

#include <locale.h>
#include <ncurses.h>

#define ffncurses_height()  getmaxy(stdscr)
#define ffncurses_width()  getmaxx(stdscr)

static inline int imax(int a, int b) { return (a > b) ? a : b; }

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
	uint modified :1;
	uint popup :1;
};

/** Initialize ncurses.
Set up UTF-8 and color support, hide cursor, initialize color pairs. */
static inline void ffncurses_init(struct ffncurses_wnd *w, struct ffncurses_conf *c)
{
	setlocale(LC_ALL, "");
	initscr();
	curs_set(0);
	start_color();
	use_default_colors();
	refresh();

	for (uint i = 0;  c->colors[i][0] != -1 || c->colors[i][1] != -1;  i++) {
		init_pair(i + 1, c->colors[i][0], c->colors[i][1]);
	}

	w->wnd = stdscr;
}

/** Terminate ncurses and restore the terminal to its normal state. */
static inline void ffncurses_end()
{
	endwin();
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
static inline void ffncurses_printn_attr(struct ffncurses_wnd *w, int y, int x, const char *text, unsigned len, unsigned attr, unsigned color_id)
{
	if (attr)
		wattron(w->wnd, attr);
	if (color_id)
		wattron(w->wnd, COLOR_PAIR(color_id));
	mvwaddnstr(w->wnd, y, x, text, len);
	if (color_id)
		wattroff(w->wnd, COLOR_PAIR(color_id));
	if (attr)
		wattroff(w->wnd, attr);
	w->modified = 1;
}

static inline void ffncurses_print_attr(struct ffncurses_wnd *w, int y, int x, const char *text, unsigned attr, unsigned color_id)
{
	ffncurses_printn_attr(w, y, x, text, strlen(text), attr, color_id);
}

/** Print text line. */
static inline void ffncurses_println_attr(struct ffncurses_wnd *w, int y, int x, const char *text, unsigned attr, unsigned color_id)
{
	mvwhline(w->wnd, y, 0, ' ', getmaxx(w->wnd));
	ffncurses_print_attr(w, y, x, text, attr, color_id);
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

/** Clear a single line.
y: Row to clear (0-79 columns). */
#define ffncurses_line_clear(w, y) \
do { \
	mvwhline((w)->wnd, y, 0, ' ', getmaxx((w)->wnd)); \
	(w)->modified = 1; \
} while (0)

/** Create a bordered popup window.
h: Height
w: Width
y, x: Top-left corner coordinates */
static inline void ffncurses_popup(struct ffncurses_wnd *w, int h, int width, int y, int x, const char *title, unsigned color_id)
{
	w->popup = 1;
	w->wnd = newwin(h, width, y, x);
	box(w->wnd, 0, 0);
	ffncurses_print_attr(w, 0, 1, title, A_BOLD, color_id);
}

static inline void ffncurses_popup_del(struct ffncurses_wnd *w)
{
	delwin(w->wnd);
	w->wnd = NULL;

	redrawwin(stdscr);
	wrefresh(stdscr);
}
