/** phiola: TUI-ncurses: Help info
2026, Simon Zolin */

static const struct {
	char key[16];
	char descr[20];
} help_keys[] = {
	{ "Space",      "Pause/Resume" },
	{ "P/N",        "Previous/Next" },
	{ ".",          "Stop" },
	{ "Left/Right", "Seek" },
	{ "G/Alt+G",    "Bookmark Set/Jump" },
	{ "Ctrl+Up/Down", "Volume" },
	{ "M",          "Mute" },
	{ "I",          "Track Info" },
	{ "Tab",        "Switch view" },
	{ "Q / F10",    "Quit" },
	{ "",           "" },
	{ "---",        "List:" },
	{ "Up/Down",    "Scroll" },
	{ "Enter",      "Play Selected" },
	{ "+",          "New" },
	{ "-",          "Close" },
	{ "S",          "Save" },
	{ "[ / ]",      "Prev/Next" },
	{ "A",          "Add URL" },
	{ "O",          "Sort Tracks" },
	{ "#",          "Jump to Track" },
	{ "Del",        "Remove Track" },
	{ "Shift+Del",  "Delete File" },
	{ "F6",         "Rename File" },
	{ "R",          "Repeat On/Off" },
	{ "Z",          "Random On/Off" },
	{ "",           "" },
	{ "---",        "Explorer:" },
	{ "A",          "Add To List" },
	{ "/",          "Jump To Dir" },
};

static void help_display()
{
	uint n = FF_COUNT(help_keys);
	uint visible = mod->wpopup.ch;
	uint col_width = FF_COUNT(help_keys[0].key);

	uint y = 1;
	for (uint i = mod->dlg.top;  i < mod->dlg.top + visible && i < n;  i++) {
		uint space = col_width - ffsz_len(help_keys[i].key);
		uint len = tui2_printf("%s%*c%s"
			, help_keys[i].key, (ffsize)space, ' '
			, help_keys[i].descr);
		ffncurses_println_attr(&mod->wpopup, y++, 1, mod->buf, len, 0, 0);
	}

	while (y <= mod->wpopup.ch) {
		ffncurses_line_clear(&mod->wpopup, y++);
	}
}

/** Return 0 if handled */
static int help_action(int k)
{
	switch (k & ~FFKEY_MODMASK) {
	case FFKEY_UP:
		if (mod->dlg.top > 0)
			mod->dlg.top--;
		break;

	case FFKEY_DOWN:
		if (mod->dlg.top + mod->wpopup.ch < FF_COUNT(help_keys))
			mod->dlg.top++;
		break;

	default:
		return 1;
	}

	help_display();
	return 0;
}

static void help_show()
{
	tui2_popup("Help", 66);
	help_display();
	mod->popup_type = POPUP_HELP;
}
