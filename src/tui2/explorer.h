/** phiola: TUI-ncurses: File Explorer
2026, Simon Zolin */

static void explorer_init()
{
	if (!mod->ex.dir)
		mod->ex.dir = core->conf.env_expand(USER_HOME);
}

static void explorer_reset()
{
	struct tui2_explorer *e = &mod->ex;
	ffdirscanx_close(&e->dx);
	ffmem_zero_obj(&e->dx);
}

static void explorer_close()
{
	struct tui2_explorer *e = &mod->ex;
	ffmem_free(e->dir);
	explorer_reset();
}

static int explorer_scan(const char *dir)
{
	struct tui2_explorer *e = &mod->ex;
	ffdirscanx dx = {};
	if (ffdirscanx_open(&dx, dir, FFDIRSCANX_SORT_DIRS)) {
		syswarnlog(NULL, "dir open: %s", dir);
		return 1;
	}

	uint ndirs = 0, isdir;
	while (ffdirscanx_next(&dx, &isdir) && isdir) {
		ndirs++;
	}
	e->dirs_n = ndirs;

	e->cur = 1; // "UP"
	ffdirscanx_close(&e->dx);
	e->dx = dx;
	return 0;
}

/* Format:
[/dir/dir]
<UP>
<DIR> dir1
<DIR> dirN
file1
fileN
*/
static void explorer_display()
{
	struct tui2_explorer *e = &mod->ex;
	uint y = Y_LIST, clr, dir, n;
	const char *fn;
	for (uint i = e->top;  i < e->top + mod->list_cap;  i++) {
		clr = (i == e->cur) ? CLR_LIST_SEL : 0;

		switch (i) {
		case 0:
			n = tui2_printf("[%s]", e->dir);
			break;

		case 1:
			n = tui2_printf("<UP>");
			break;

		default:
			if (!(fn = ffdirscanx_at(&e->dx, i - 2, &dir)))
				goto end;
			n = (dir)
				? tui2_printf("<DIR> %s", fn)
				: tui2_printf("%s", fn);
		}

		ffncurses_println_attr(&mod->wmain, y++, 0, mod->buf, n, 0, clr);
	}

end:
	while (y < Y_LIST + mod->list_cap) {
		ffncurses_line_clear(&mod->wmain, y++);
	}
}

static int explorer_navigate(const char *dir)
{
	struct tui2_explorer *e = &mod->ex;
	if (explorer_scan(dir))
		return 1;
	ffmem_free(e->dir);
	e->dir = ffsz_dup(dir);
	explorer_display();
	return 0;
}

static void explorer_scroll_abs(uint cur)
{
	struct tui2_explorer *e = &mod->ex;
	uint n = ffdirscan_count(&e->dx.ds) + 2;
	cur = ffmin(cur, n - 1);

	if (cur < e->top)
		e->top = cur;
	else if (cur >= e->top + mod->list_cap)
		e->top = cur - mod->list_cap + 1;

	e->cur = cur;
	explorer_display();
}

static void explorer_scroll_by(int by)
{
	explorer_scroll_abs(ffmax((int)mod->ex.cur + by, 0));
}

static void explorer_exec(uint add)
{
	struct tui2_explorer *e = &mod->ex;
	char *filepath;

	if (e->cur == 0) {
		return; // Action on directory path

	} else if (e->cur == 1) {
		// Action on "<UP>"
		ffstr parent;
		ffpath_splitpath_str(FFSTR_Z(e->dir), &parent, NULL);
		if (!parent.len)
			ffstr_setz(&parent, "/");
		filepath = ffsz_allocfmt("%S", &parent);

	} else {
		uint cur = e->cur - 2;
		const char *fn = ffdirscanx_at(&e->dx, cur, NULL);

		filepath = ffsz_allocfmt("%s%s%s"
			, e->dir, (e->dir[1]) ? "/" : "", fn);

		if (add || cur >= e->dirs_n) {

			// Add file/dir to the current list
			struct phi_queue_entry qe = {
				.url = filepath,
			};
			int i = mod->queue->add(NULL, &qe);
			if (!add)
				mod->queue->play(NULL, mod->queue->at(NULL, i));
			ffmem_free(filepath);
			return;
		}
	}

	// Navigate to the directory
	if (explorer_scan(filepath)) {
		ffmem_free(filepath);
		return;
	}
	ffmem_free(e->dir);
	e->dir = filepath;
	explorer_display();
}

/** Return 0 if handled */
static int explorer_jump_action(int k)
{
	int r = tui2_dialog_edit_action(k);
	if (r == FFKEY_ENTER) {
		uint len = mod->dlg.len - 1;
		if (!len)
			return r;

		mod->dlg.buf[len] = '\0';
		if (explorer_navigate(mod->dlg.buf)) {
			mod->dlg.buf[len] = '_';
		}
	}

	return r;
}

/** Return 0 if handled */
static int explorer_action(int k, int key)
{
	// struct tui2_explorer *e = &mod->ex;
	switch (key) {
	case '/':
		tui2_dialog_edit_show("Enter directory path:", 40, 0);
		mod->popup_type = POPUP_EXPLORERJUMP;
		break;

	case 'A':
	case FFKEY_ENTER:
		explorer_exec(key == 'A');  break;

	case FFKEY_UP:
	case FFKEY_DOWN:
		explorer_scroll_by((key == FFKEY_DOWN) ? 1 : -1);  break;

	case FFKEY_HOME:
	case FFKEY_END:
		explorer_scroll_abs((key == FFKEY_HOME) ? 0 : ~0U);  break;

	case FFKEY_PGUP:
	case FFKEY_PGDN:
		explorer_scroll_by((key == FFKEY_PGDN) ? (int)mod->list_cap : -(int)mod->list_cap);  break;

	default:
		return 1;
	}

	return 0;
}
