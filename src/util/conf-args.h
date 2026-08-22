/** phiola: ffconf-ffargs bridge
2023, Simon Zolin */

#include <util/conf-obj.h>
#include <util/conf-write.h>
#include <ffbase/args.h>

/** Process conf data.
options: enum FFARGS_OPT
Return 0 on success;
  <0: enum FFARGS_E;
  >0: error code from a user function */
static inline int ffargs_process_conf(struct ffargs *as, const struct ffarg *scheme, void *obj, ffuint options,
	ffstr conf)
{
	as->ax.scheme = scheme;
	as->ax.obj = obj;
	as->options = options;

	struct ffconf_obj c = {};
	int (*on_done)(void*);
	const struct ffarg *a = NULL;
	int expecting_value = 0;
	ffstr arg, key = {};
	int r;
	for (;;) {

		int rc = ffconf_obj_read(&c, &conf, &arg);
		if (rc == FFCONF_MORE) {
			break;

		} else if (rc == FFCONF_ERROR) {
			r = _ffargs_err(as, FFARGS_E_ARG, "%s", ffconf_error(&c.lt));
			goto end;

		} else if ((rc == FFCONF_VAL || rc == FFCONF_VAL_NEXT) && !expecting_value) {
			r = _ffargs_err(as, FFARGS_E_VAL, "not expecting values here");
			goto end;

		} else if (!(rc == FFCONF_VAL || rc == FFCONF_VAL_NEXT) && expecting_value) {
			break;

		} else if (rc == FFCONF_OBJ_OPEN) {
			continue;

		} else if (rc == FFCONF_OBJ_CLOSE) {
			as->ax.scheme = scheme;
			as->ax.obj = obj;
			continue;
		}

		if (expecting_value) {
			expecting_value = 0;
			r = _ffargs_value(as, a, key, arg);
			if (r) goto end;
			continue;
		}

		for (uint ir = 0; ; ir++) {
			FF_ASSERT(ir < 100); (void)ir;
			if (!(a = _ffargs_find(as, arg, options))) {
				r = -FFARGS_E_ARG;
				goto end;
			}

			int r = _ffargs_arg(as, a, arg);
			if (r == -FFARGS_E_VAL) {
				expecting_value = 1;
				key = arg;
			} else if (r == -FFARGS_E_REDIR) {
				continue;
			} else if (r) {
				goto end;
			}
			break;
		}
	}

	if (expecting_value) {
		r = _ffargs_err(as, FFARGS_E_VAL, "expecting value after '%S'", &key);
		goto end;
	}

	on_done = (int(*)(void*))_ffarg_ctx_done(&as->ax, 0)->value;
	r = (on_done) ? on_done(as->ax.obj) : 0;

end:
	if (r) {
		// add the current "line:char" info to the beginning of error message
		char buf[250];
		ffsz_format(buf, sizeof(buf), "%u:%u: %s"
			, (int)ffconf_line(&c.lt), (int)ffconf_col(&c.lt), as->error);
		ffsz_copyz(as->error, sizeof(as->error), buf);
	}

	ffconf_obj_fin(&c);
	return r;
}

/** Write conf data (single level only).
Skip the fields whose types are not supported.
Return 0 on success */
static inline int ffarg_write_conf(ffconfw *cw, const struct ffarg *ctx, const void *obj)
{
	int r = 0;
	for (const struct ffarg *a = ctx;  a->name[0];  a++) {
		ffuint off = (ffsize)a->value;
		if (off >= _FFARG_MAX_OFF)
			continue;
		ffuint buf_len_prev = cw->buf.len;
		r |= (0 > ffconfw_add_keyz(cw, a->name));
		const void *ptr = FF_PTR(obj, off);
		switch (_FFARG_TYPE(a)) {
		case 'b':
			r |= (0 > ffconfw_add_uint(cw, *(ffbyte*)ptr));  break;

		case 'u':
			r |= (0 > ffconfw_add_uint(cw, *(ffuint*)ptr));  break;

		case 'U':
			r |= (0 > ffconfw_add_uint(cw, *(ffuint64*)ptr));  break;

		case 'd':
			r |= (0 > ffconfw_add_int(cw, *(int*)ptr));  break;

		case 'D':
			r |= (0 > ffconfw_add_int(cw, *(ffint64*)ptr));  break;

		case 'F':
			r |= (0 > ffconfw_add_float(cw, *(double*)ptr, 6));  break;

		case 's':
			r |= (0 > ffconfw_add_strz(cw, *(char**)ptr));  break;

		case 'S':
			r |= (0 > ffconfw_add_str(cw, *(ffstr*)ptr));  break;

		default:
			cw->buf.len = buf_len_prev; // Remove the previously written key
		}
	}
	return r;
}
