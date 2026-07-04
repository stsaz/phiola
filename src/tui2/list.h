/** phiola: TUI-ncurses: Playlist
2026, Simon Zolin */

static void list_init()
{
	struct tui2_list *l = &mod->list;
	l->counter = 1;
	struct phi_queue_conf *qc = mod->queue->conf(NULL);
	l->q_guard = qc->first_filter;
	qc->name = ffsz_allocfmt("Playlist %u", 1);
}

static void list_close()
{
	struct tui2_list *l = &mod->list;
	core->timer(0, &l->tmr_list_redraw, 0, NULL, NULL);
}

static void list_display()
{
	struct tui2_list *l = &mod->list;
	uint y = Y_LIST, n;
	for (uint i = l->top;  i < l->top + mod->list_cap;  i++) {
		const struct phi_queue_entry *qe = mod->queue->at(NULL, i);
		if (!qe)
			break;

		ffstr artist = {}, title = {};
		core->metaif->find(&qe->meta, FFSTR_Z("artist"), &artist, 0);
		core->metaif->find(&qe->meta, FFSTR_Z("title"), &title, 0);

		if (title.len) {
			n = tui2_printf("%u. %S - %S", i + 1, &artist, &title);
		} else {
			ffpath_split3_str(FFSTR_Z(qe->url), NULL, &title, NULL); // Use file name as title
			n = tui2_printf("%u. %S", i + 1, &title);
		}

		ffncurses_line_clear(&mod->wmain, y);
		uint clr = (i == l->cur) ? CLR_LIST_SEL : 0;
		ffncurses_printn_attr(&mod->wmain, y++, 0, mod->buf, n, 0, clr);
	}

	while (y < Y_LIST + mod->list_cap) {
		ffncurses_line_clear(&mod->wmain, y++);
	}
}

static void list_redraw_delayed(void *param)
{
	struct tui2_list *l = &mod->list;
	list_display();
	ffncurses_update(&mod->wmain);
	l->redrawing = 0;
}

static void q_on_change(phi_queue_id q, uint flags, uint pos)
{
	struct tui2_list *l = &mod->list;
	if (mod->view_explorer)
		return; // Ignore list updates in Explorer view

	switch (flags) {
	case 'n':
		mod->queue->qselect(q);
		list_view_title();
		list_redraw_delayed(NULL);
		break;

	case 'd':
		if (q == mod->q_active)
			mod->q_active = NULL;
		list_view_title();
		list_redraw_delayed(NULL);
		break;

	case 'a':
	case 'r':
	case 'c':
		if (q != mod->queue->select(PHI_QSEL_CUR))
			break; // An inactive list has been modified
		if (!l->redrawing) {
			l->redrawing = 1;
			core->timer(0, &l->tmr_list_redraw, -50, list_redraw_delayed, (void*)(ffsize)flags);
		}
		break;
	}
}

static void list_scroll_abs(uint cur)
{
	struct tui2_list *l = &mod->list;
	uint n = mod->queue->count(NULL);
	if (!n)
		return;
	cur = ffmin(cur, n - 1);

	if (cur < l->top)
		l->top = cur;
	else if (cur >= l->top + mod->list_cap)
		l->top = cur - mod->list_cap + 1;

	l->cur = cur;
	list_display();
}

static void list_scroll_by(int by)
{
	list_scroll_abs(ffmax((int)mod->list.cur + by, 0));
}

/** Return 0 if handled */
static int list_jump_action(int k)
{
	int r = tui2_dialog_edit_action(k);
	if (r == FFKEY_ENTER) {
		uint n, len = mod->dlg.len - 1;
		if (!len
			|| len != ffs_toint(mod->dlg.buf, len, &n, FFS_INT32))
			return r;

		n--;
		struct phi_queue_entry *qe = mod->queue->at(NULL, n);
		mod->queue->play(NULL, qe);
	}
	return r;
}

static phi_queue_id list_create()
{
	struct tui2_list *l = &mod->list;
	struct phi_queue_conf qc = {
		.name = ffsz_allocfmt("Playlist %u", ++l->counter),
		.first_filter = l->q_guard,
		.ui_module = "tui.play",
	};
	return mod->queue->create(&qc); // -> on_change('n')
}

/** Return 0 if handled */
static int list_action(int k, int key)
{
	struct tui2_list *l = &mod->list;

	switch (key) {

	case '+':
		list_create();  break;

	case '-':
		if (mod->queue->total() == 1)
			mod->queue->clear(NULL);
		else
			mod->queue->destroy(NULL); // -> on_change('d')
		break;

	case '[':
	case ']':
		mod->queue->select((key == '[') ? PHI_QSEL_PREV : PHI_QSEL_NEXT);
		list_view_title();
		list_display();
		break;

	case '#':
		tui2_dialog_edit_show("Enter track number:", 20, 1);
		mod->popup_type = POPUP_LISTJUMP;
		break;

	case 'R': {
		struct phi_queue_conf *qc = mod->queue->conf(NULL);
		qc->repeat_all = !qc->repeat_all;
		tui2_status("Repeat: %s", (qc->repeat_all) ? "on" : "off");
		break;
	}

	case 'Z': {
		struct phi_queue_conf *qc = mod->queue->conf(NULL);
		qc->random = !qc->random;
		tui2_status("Random: %s", (qc->random) ? "on" : "off");
		break;
	}

	case FFKEY_ENTER: {
		struct phi_queue_entry *qe = mod->queue->at(NULL, l->cur);
		mod->queue->play(NULL, qe);
		break;
	}

	case FFKEY_DEL:
		mod->queue->remove_at(NULL, l->cur, 1);  break;

	case FFKEY_HOME:
	case FFKEY_END:
		list_scroll_abs((key == FFKEY_HOME) ? 0 : ~0U);
		break;

	case FFKEY_PGUP:
	case FFKEY_PGDN:
		list_scroll_by((key == FFKEY_PGDN) ? (int)mod->list_cap : -(int)mod->list_cap);
		break;

	case FFKEY_UP:
	case FFKEY_DOWN:
		list_scroll_by((key == FFKEY_DOWN) ? 1 : -1);  break;

	default:
		return 1;
	}

	return 0;
}

static void list_save_complete(void *param, phi_track *t)
{
	struct tui2_list *l = &mod->list;
	if (--l->save_pending == 0)
		tui2_exit();
}

static char* list_name(uint i)
{
	return ffsz_allocfmt("%s" AUTO_LIST_FN, mod->user_conf_dir, i);
}

/** Save playlists to disk.
Return non-zero if async operations are pending. */
static int lists_save()
{
	struct tui2_list *l = &mod->list;
	if (ffdir_make(mod->user_conf_dir)
		&& !fferr_exist(fferr_last())) {
		syserrlog("dir make: %s", mod->user_conf_dir);
		return 0;
	}

	char *fn = NULL;
	uint i = 0, n = mod->queue->total();
	for (;  i < n;  i++) {
		phi_queue_id q = mod->queue->get(i);
		ffmem_free(fn);
		fn = list_name(i + 1);
		if (!mod->queue->save(q, fn, list_save_complete, NULL))
			l->save_pending++;
	}

	ffmem_free(fn);
	fn = list_name(i + 1);
	fffile_remove(fn);

	ffmem_free(fn);
	return l->save_pending;
}

/** Load playlists from disk */
static void lists_load()
{
	char *fn = NULL;
	for (uint i = 1;;  i++) {
		fn = list_name(i);
		fffileinfo fi;
		if (fffile_info_path(fn, &fi))
			break;
		fftime mt = fffileinfo_mtime(&fi);

		uint mt_set = 1;
		phi_queue_id q = NULL;
		if (i == 1) {
			// Don't set `last_mod_time` if there are tracks added from command line.
			// This prevents `m3u-read` from setting `modified=0` on the queue.
			mt_set = (mod->queue->count(q) == 0);
		} else {
			q = list_create();
		}

		if (mt_set) {
			mt.sec += FFTIME_1970_SECONDS;
			mod->queue->conf(q)->last_mod_time = mt;
		}

		struct phi_queue_entry qe = {
			.url = fn,
		};
		fn = NULL;
		mod->queue->add(q, &qe);
	}
	ffmem_free(fn);
}
