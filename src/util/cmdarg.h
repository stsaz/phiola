/** Process command-line arguments
2023, Simon Zolin */

struct cmd_arg;
struct cmd_ctx {
	const struct cmd_arg *scheme;
	void *obj;
};

enum CMDO {
	/** Match arguments by just prefix */
	CMDO_PARTIAL = 1,
	/** Enforce policy: no duplicate arguments */
	CMDO_DUPLICATES = 2,
};

struct cmd_obj {
	char **argv;
	uint argc;
	uint argi;
	struct cmd_ctx cx;
	uint options; // enum CMDO
	uint used_bits[2];
	char error[250];
};

struct cmd_arg {
	/** Name (case-sensitive, in ascending alphabetic order) of the argument */
	char name[16];

	/**
	'\0' function

	'1' switch, byte
	'u' unsigned int-32
	'd' signed int-32
	'U' unsigned int-64
	'D' signed int-64

	's' char* NULL-terminated string
	'S' ffstr string

	'>' new sub-context
	'{' new sub-context function
	*/
	uint flags;

	/** Function pointer or a struct-field offset */
	const void *value;
};

#define _CMDARG_TYPE(a)  (a->flags & 0xff)
#define _CMDARG_MULTI(a)  ((a->flags & 0xff00) >> 8 == '+')

typedef int (*cmdarg_action_t)(void *obj);

static int cmdarg_err(struct cmd_obj *c, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	ffs_formatv(c->error, sizeof(c->error), fmt, va);
	va_end(va);
	return -1;
}

union cmd_val {
	char *b;
	int *i32;
	ffstr *s;
	char **sz;

	int (*f_sw)(void*);
	int (*f_str)(void*, ffstr);
	int (*f_sz)(void*, const char*);
	int (*f_int)(void*, uint64);
	struct cmd_ctx (*f_ctx)(void*);
};

static int _cmdarg_handle_value(struct cmd_obj *c, const struct cmd_arg *ca, const char *arg_name, ffstr val)
{
	struct cmd_ctx *cx = &c->cx;

	const ffuint MAX_OFF = 64*1024;
	ffsize off = (ffsize)ca->value;
	union cmd_val u;
	u.b = (off < MAX_OFF) ? FF_PTR(cx->obj, off) : (void*)ca->value;

	uint t = _CMDARG_TYPE(ca);
	uint64 i = 0;
	uint f = 0;
	switch (t) {

	case 'S':
		if (off < MAX_OFF) {
			*u.s = val;
		} else {
			return u.f_str(cx->obj, val);
		}
		break;

	case 's':
		if (off < MAX_OFF) {
			*u.sz = val.ptr;
		} else {
			return u.f_sz(cx->obj, val.ptr);
		}
		break;

	case 'D':
	case 'd':
		f |= FFS_INTSIGN;
		// fallthrough
	case 'U':
	case 'u':
		if (t == 'D' || t == 'U')
			f |= FFS_INT64;
		else
			f |= FFS_INT32;
		if (!ffstr_toint(&val, &i, f))
			return cmdarg_err(c, "expected integer value for '%s', got '%S'", arg_name, &val);
		if (off < MAX_OFF) {
			*u.i32 = i;
		} else {
			return u.f_int(cx->obj, i);
		}
		break;
	}

	return 0;
}

static int _cmd_handle(struct cmd_obj *c, const struct cmd_arg *ca)
{
	struct cmd_ctx *cx = &c->cx;
	const char *arg_name = c->argv[c->argi];

	const ffuint MAX_OFF = 64*1024;
	ffsize off = (ffsize)ca->value;
	union cmd_val u;
	u.b = (off < MAX_OFF) ? FF_PTR(cx->obj, off) : (void*)ca->value;

	ffuint k = ca - c->cx.scheme;
	if ((c->options & CMDO_DUPLICATES)
		&& k < sizeof(c->used_bits)*8
		&& ffbit_array_set(&c->used_bits, k)
		&& !_CMDARG_MULTI(ca))
		return cmdarg_err(c, "duplicate value for option '%s'", ca->name);

	switch (_CMDARG_TYPE(ca)) {
	case '{':
		c->cx = u.f_ctx(cx->obj);
		ffmem_zero_obj(c->used_bits);
		return 0;

	case '>':
		c->cx.scheme = ca->value;
		ffmem_zero_obj(c->used_bits);
		return 0;

	case '1':
		if (off < MAX_OFF)
			*u.b = 1;
		else
			return u.f_sw(cx->obj);
		return 0;

	case 0:
		if (u.f_sw)
			return u.f_sw(cx->obj);
		return 0;

	case 'S': case 's':
	case 'D': case 'U':
	case 'd': case 'u':
		break;

	default:
		FF_ASSERT(0);
		return -1;
	}

	if (c->argi + 1 == c->argc)
		return cmdarg_err(c, "expecting a value after '%s'", ca->name);
	c->argi++;
	return _cmdarg_handle_value(c, ca, arg_name, FFSTR_Z(c->argv[c->argi]));
}

static inline int ffsz_cmp_n(const char *sz, const char *cmpz)
{
	ffsize i = 0;
	do {
		if (sz[i] != cmpz[i])
			return ((ffbyte)sz[i] < (ffbyte)cmpz[i]) ? -(int)(i+1) : (int)(i+1);
	} while (sz[i++] != '\0');
	return 0;
}

static const struct cmd_arg* _cmdarg_any(struct cmd_ctx *cx, uint i)
{
	for (;  cx->scheme[i].name[0] != '\0';  i++) {}

	if (cx->scheme[i].name[1] == '\1')
		return &cx->scheme[i];
	return NULL;
}

static int cmd_process(struct cmd_obj *c)
{
	uint i = 0;
	for (;;) {

		if (c->argi == c->argc) { // no more arguments
			goto done;
		}
		const char *a = c->argv[c->argi];

		const struct cmd_arg *ca = &c->cx.scheme[i];
		if (ca->name[0] == '\0') { // the current argument didn't match
			goto no_match;
		}

		int r = ffsz_cmp_n(a, ca->name);
		if (r < 0) {
			if (c->options & CMDO_PARTIAL) {
				int k = -r - 1;
				if (a[k] == '\0') {
					if (c->cx.scheme[i+1].name[0] == '\0'
						|| !ffsz_matchz(c->cx.scheme[i+1].name, a))
						r = 0;
				}
			}
			if (!!r)
				goto no_match;
		}

		if (!r) {
			if (0 != (r = _cmd_handle(c, ca)))
				return r;

			c->argi++;
			i = 0;
			continue;
		}

		i++;
		continue;

no_match:
		ca = _cmdarg_any(&c->cx, i);
		if (ca) {
			union cmd_val u = { (void*)ca->value };
			switch (_CMDARG_TYPE(ca)) {
			case '{':
				c->cx = u.f_ctx(c->cx.obj);
				ffmem_zero_obj(c->used_bits);
				break;

			case '>':
				c->cx.scheme = ca->value;
				ffmem_zero_obj(c->used_bits);
				break;

			default:
				if (!!(r = _cmdarg_handle_value(c, ca, NULL, FFSTR_Z(a))))
					return r;
				c->argi++;
			}

			i = 0;
			continue;
		}
		return cmdarg_err(c, "unknown argument '%s'.  Use '-h' for usage info.", c->argv[c->argi]);
	}

done:
	for (;  c->cx.scheme[i].name[0] != '\0';  i++) {}

	if (c->cx.scheme[i].name[1] == '\1')
		i++;

	cmdarg_action_t on_done = c->cx.scheme[i].value;
	if (on_done)
		return on_done(c->cx.obj);
	return 0;
}
