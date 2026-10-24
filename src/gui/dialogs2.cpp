/** phiola: GUI dialogs */

#include <phiola.h>
#include <util/util.hpp>
#include <gui/gui.h>

/** Show @-var suggestions after '@' is typed in the edit control */
static inline void edit_changed(ffui_menuxx *m, ffui_editxx *e, ffui_window *w, const char *const *suggestions, uint n, uint id_first)
{
	xxvec v(e->text());
	if (!(v.len && v.str().last_char() == '@'))
		return;

	if (!m->h) {
		ffui_menu_create_popup(m, w);
		for (uint i = 0;  i < n;  i++) {
			ffui_menu_itemadd(m, suggestions[i], id_first + i);
		}
	}

	ffui_menu_popup_under(m, (ffui_ctl*)e);
}

/** Add selected @-var text to the edit control */
static inline void edit_menu_clicked(ffui_editxx *e, const char *const *suggestions, uint i)
{
	xxvec v(e->text());
	const char *s = suggestions[i] + 1;
	e->text(v.add(s).str());
	ffui_edit_pos_set(e, -1);
}

#include <gui/record.hpp>
#include <gui/convert.hpp>
