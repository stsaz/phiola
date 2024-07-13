/** phiola: ffconf-ffargs bridge
2023, Simon Zolin */

#include <ffgui/conf-obj.h>
#include <ffbase/args.h>

/** Process conf data.
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

	int (*on_done)(void*) = (int(*)(void*))_ffarg_ctx_done(&as->ax, 0)->value;
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
