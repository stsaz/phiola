/** phiola: executor
2025, Simon Zolin */

#include <ffbase/args.h>

struct conf {
	uint	codepage_id;
	u_char	allow_hibernate;
};

static int ffu_coding(ffstr s)
{
	static const u_char codepage_val[] = {
		FFUNICODE_WIN1251,
		FFUNICODE_WIN1252,
		FFUNICODE_WIN866,
	};
	static const char codepage_str[][8] = {
		"win1251",
		"win1252",
		"win866",
	};
	int r = ffcharr_findsorted(codepage_str, FF_COUNT(codepage_str), sizeof(codepage_str[0]), s.ptr, s.len);
	if (r < 0)
		return -1;
	return codepage_val[r];
}

static int conf_codepage(struct conf *c, ffstr s)
{
	int r = ffu_coding(s);
	if (r < 0)
		return -1;
	c->codepage_id = r;
	return 0;
}

#define O(m)  (void*)FF_OFF(struct conf, m)
static const struct ffarg conf_args[] = {
	{ "AllowHibernate",	'1',	O(allow_hibernate) },
	{ "Codepage",		'S',	conf_codepage },
	{}
};
#undef O
