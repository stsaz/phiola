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

		if (url_checkz(qe->url)) {
			n = tui2_printf("%u. %s", i + 1, qe->url);

		} else {
			ffstr artist = {}, title = {};
			core->metaif->find(&qe->meta, FFSTR_Z("artist"), &artist, 0);
			core->metaif->find(&qe->meta, FFSTR_Z("title"), &title, 0);
			if (title.len) {
				n = tui2_printf("%u. %S - %S", i + 1, &artist, &title);
			} else {
				ffpath_split3_str(FFSTR_Z(qe->url), NULL, &title, NULL); // Use file name as title
				n = tui2_printf("%u. %S", i + 1, &title);
			}
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

	case 'm':
	case 'a':
	case 'r':
	case 'u':
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

/** Return 0 if handled */
static int list_save_action(int k)
{
	int r = tui2_dialog_edit_action(k);
	if (r == FFKEY_ENTER) {
		uint len = mod->dlg.len - 1;
		if (len) {
			char *fn = mod->dlg.buf;
			fn[len] = '\0';
			if (fffile_exists(fn)) {
				errlog("file exists: %s", fn);
				return r;
			}
			mod->queue->save(NULL, fn, NULL, NULL);
		}
	}
	return r;
}

/** Return 0 if handled */
static int list_frename_action(int k)
{
	struct tui2_list *l = &mod->list;
	int r = tui2_dialog_edit_action(k);
	if (r == FFKEY_ENTER) {
		uint len = mod->dlg.len - 1;
		if (len) {
			char *name = mod->dlg.buf;
			name[len] = '\0';
			struct phi_queue_entry *qe = mod->queue->at(NULL, l->cur);
			ffstr path, ext;
			ffpath_split3_str(FFSTR_Z(qe->url), &path, NULL, &ext);
			char *fn = ffsz_allocfmt("%S%s%s.%S"
				, &path, PATH_SLASH, name, &ext);
			mod->queue->rename(qe, fn, PHI_QRN_ACQUIRE);
		}
	}
	return r;
}

/** Return 0 if handled */
static int list_addurl_action(int k)
{
	int r = tui2_dialog_edit_action(k);
	if (r == FFKEY_ENTER) {
		uint len = mod->dlg.len - 1;
		if (len) {
			mod->dlg.buf[len] = '\0';
			struct phi_queue_entry qe = {
				.url = mod->dlg.buf,
			};
			mod->queue->add(NULL, &qe);
		}
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

static int file_trash(uint i)
{
	struct phi_queue_entry *qe = mod->queue->at(NULL, i);
	int r;
	const char *e;

#ifdef FF_WIN
	r = ffui_file_del((const char *const *)&qe->url, 1, FFUI_FILE_TRASH);
	e = fferr_strptr(fferr_last());
#else
	r = ffui_glib_trash(qe->url, &e);
#endif

	if (r) {
		errlog("moving file to trash: %s: %s"
			, qe->url, e);
	}
	return r;
}

static const char sort_options[][24] = {
	"Sort by File Name",
	"Sort by File Size",
	"Sort by File Date",
	"Sort by Tag Artist",
	"Sort by Tag Date",
	"Shuffle",
};

static const u_char sort_q_cmd[] = {
	PHI_Q_SORT_FILENAME,
	PHI_Q_SORT_FILESIZE,
	PHI_Q_SORT_FILEDATE,
	PHI_Q_SORT_TAG_ARTIST,
	PHI_Q_SORT_TAG_DATE,
	PHI_Q_SORT_RANDOM,
};

static void list_sort_display()
{
	uint visible = mod->wpopup.ch;
	uint n = ffmin(mod->dlg.top + visible, FF_COUNT(sort_options));
	uint y = 1;
	for (uint i = mod->dlg.top;  i < n;  i++) {
		int mark = (i == mod->dlg.cur) ? '>' : ' ';
		uint r = tui2_printf("%c %s", mark, sort_options[i]);
		tui2_popup_println(y++, mod->buf, r, 0, 0);
	}
}

/** Return 0 if handled */
static int list_sort_action(int k)
{
	switch (k & ~FFKEY_MODMASK) {
	case FFKEY_UP:
		if (mod->dlg.cur > 0)
			mod->dlg.cur--;
		break;

	case FFKEY_DOWN:
		if (mod->dlg.cur < FF_COUNT(sort_options) - 1)
			mod->dlg.cur++;
		break;

	case FFKEY_ENTER:
		mod->queue->sort(NULL, sort_q_cmd[mod->dlg.cur]);
		return 1;

	default:
		return 1;
	}

	list_sort_display();
	return 0;
}

/** Return 0 if handled */
static int list_action(int k, int key)
{
	struct tui2_list *l = &mod->list;

	switch (key) {

	case FFKEY_F6: {
		const struct phi_queue_entry *qe = mod->queue->at(NULL, l->cur);
		ffstr name;
		ffpath_split3_str(FFSTR_Z(qe->url), NULL, &name, NULL);
		tui2_dialog_edit_show("Enter new file name:", 33, 0, name);
		mod->popup_type = POPUP_LIST_FRENAME;
		break;
	}

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
		tui2_dialog_edit_showz("Enter track number:", 20, 1, NULL);
		mod->popup_type = POPUP_LIST_JUMP;
		break;

	case 'A':
		tui2_dialog_edit_showz("Add URL:", 33, 0, "");
		mod->popup_type = POPUP_LIST_ADDURL;
		break;

	case 'O':
		tui2_popup("Sort Playlist", 40);
		mod->popup_type = POPUP_LIST_SORT;
		list_sort_display();
		break;

	case 'S':
		tui2_dialog_edit_showz("Save playlist to file:", 33, 0, NULL);
		mod->popup_type = POPUP_LIST_SAVE;
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
		if ((k & FFKEY_MODMASK) == FFKEY_SHIFT) {
			if (file_trash(l->cur))
				break;
		}
		mod->queue->remove_at(NULL, l->cur, 1);
		break;

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
