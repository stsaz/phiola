/** C++ cross-platform GUI interface
2023, Simon Zolin */

struct ffui_labelxx : ffui_label {
	void	text(const char *sz) { ffui_send_lbl_settext(this, sz); }
#ifdef FF_LINUX
	void	markup(const char *sz) { gtk_label_set_markup(GTK_LABEL(h), sz); }
#endif
};

struct ffui_editxx : ffui_edit {
	ffstr	text() { return ffui_edit_text(this); }
	void	text(const char *sz) { ffui_edit_settextz(this, sz); }
	void	text(ffstr s) { ffui_edit_settextstr(this, &s); }

	void	sel_all() { ffui_edit_selall(this); }

	void	focus() { ffui_ctl_focus(this); }
};

struct ffui_buttonxx : ffui_btn {
	void	text(const char *sz) { ffui_btn_settextz(this, sz); }
	void	enable(bool val) { ffui_ctl_enable(this, val); }
};

struct ffui_checkboxxx : ffui_checkbox {
	void	check(bool val) { ffui_checkbox_check(this, val); }
	bool	checked() { return ffui_send_checkbox_checked(this); }
};

struct ffui_comboboxxx : ffui_combobox {
	void	add(const char *text) { ffui_combobox_add(this, text); }
	void	clear() { ffui_combobox_clear(this); }
	ffstr	text() { return ffui_combobox_text_active(this); }

	void	set(int index) { ffui_combobox_set(this, index); }
	int		get() { return ffui_combobox_active(this); }
};

struct ffui_trackbarxx : ffui_trkbar {
	void	set(u_int value) { ffui_post_trk_set(this, value); }
	u_int	get() { return ffui_trk_val(this); }

	void	range(u_int range) { ffui_post_trk_setrange(this, range); }
};

struct ffui_tabxx : ffui_tab {
	void	add(const char *sz) { ffui_send_tab_ins(this, sz); }
	void	del(u_int i) { ffui_tab_del(this, i); }
	void	select(u_int i) { ffui_send_tab_setactive(this, i); }
	u_int	changed() { return ffui_tab_changed_index(this); }
	u_int	count() { return ffui_tab_count(this); }
};

struct ffui_statusbarxx : ffui_stbar {
	void	text(const char *sz) { ffui_send_stbar_settextz(this, sz); }
};

struct ffui_viewitemxx : ffui_viewitem {
	ffui_viewitemxx(ffstr s) { ffmem_zero_obj(this); ffui_view_settextstr(this, &s); }
};

struct ffui_viewcolxx {
	ffui_viewcol vc;

	u_int	width() { return ffui_viewcol_width(&vc); }
	void	width(u_int val) { ffui_viewcol_setwidth(&vc, val); }
};

struct ffui_viewxx : ffui_view {
	int		append(ffstr text) {
		ffui_viewitem vi = {};
		ffui_view_settextstr(&vi, &text);
		return ffui_view_append(this, &vi);
	}
	void	set(int idx, int col, ffstr text) {
		ffui_viewitem vi = {};
		ffui_view_setindex(&vi, idx);
		ffui_view_settextstr(&vi, &text);
		ffui_view_set(this, col, &vi);
	}
	void	update(u_int first, int delta) { ffui_post_view_setdata(this, first, delta); }
	void	length(u_int n, bool redraw) { ffui_view_setcount(this, n, redraw); }
	void	clear() { ffui_post_view_clear(this); }
	int		focused() { return ffui_view_focused(this); }

	ffslice	selected() { return ffui_view_selected(this); }
	int		selected_first() { return ffui_view_selected_first(this); }

	ffui_viewcolxx& column(int pos, ffui_viewcolxx *vc) { ffui_view_col(this, pos, &vc->vc); return *vc; }
	void	column(u_int pos, ffui_viewcolxx &vc) { ffui_view_setcol(this, pos, &vc.vc); }

	u_int	scroll_vert() { return ffui_send_view_scroll(this); }
	void	scroll_vert(u_int val) { ffui_post_view_scroll_set(this, val); }

#ifdef FF_LINUX
	void	drag_drop_init(u_int action_id) { ffui_view_dragdrop(this, action_id); }
#endif
};

struct ffui_windowxx : ffui_wnd {
	void	show(bool show) { ffui_show(this, show); }
	void	title(const char *sz) { ffui_send_wnd_settext(this, sz); }
	void	close() { ffui_wnd_close(this); }

	ffui_pos pos() { ffui_pos p; ffui_wnd_placement(this, &p); return p; }
	void	place(const ffui_pos &pos) { ffui_wnd_setplacement(this, SW_SHOWNORMAL, &pos); }
};
