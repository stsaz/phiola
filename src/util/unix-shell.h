/** fcom: UNIX shell utils
2022, Simon Zolin */

/*
ffui_glib_trash
ffui_exec
ffui_openfolder1
*/

#pragma once
#include <ffsys/base.h>

#ifdef FF_LINUX

#include <ffsys/process.h>
#include <ffsys/dylib.h>

#define LIBGIO_PATH  "/lib64/libgio-2.0.so.0"
#define LIBGOBJECT_PATH  "/lib64/libgobject-2.0.so.0"
#ifdef FF_ARM64
	#define LIBGIO_PATH_ALT  "/usr/lib/aarch64-linux-gnu/libgio-2.0.so.0"
	#define LIBGOBJECT_PATH_ALT  "/usr/lib/aarch64-linux-gnu/libgobject-2.0.so.0"
#else
	#define LIBGIO_PATH_ALT  "/usr/lib/x86_64-linux-gnu/libgio-2.0.so.0"
	#define LIBGOBJECT_PATH_ALT  "/usr/lib/x86_64-linux-gnu/libgobject-2.0.so.0"
#endif

/** Move file to Trash via libgio.
error: (Optional) Error message
Note: for multi-thread usage it's better first to initialize the data with:
  ffui_glib_trash("", NULL).
Note: libgio & libgobject .so descriptors are never closed. */
static inline int ffui_glib_trash(const char *path, const char **error)
{
	static ffdl gio, gobject;
	static int (*_g_file_trash)(void*, void*, void**);
	static void* (*_g_file_new_for_path)(const char*);
	static void (*_g_object_unref)(void*);

	const char *e = NULL;

	if (_g_object_unref == NULL) {
		if (gio == FFDL_NULL) {
			if (FFDL_NULL == (gio = ffdl_open(LIBGIO_PATH, 0))) {
				if (FFDL_NULL == (gio = ffdl_open(LIBGIO_PATH_ALT, 0))) {
					e = "can't open " LIBGIO_PATH;
					goto err;
				}
			}
		}

		_g_file_new_for_path = (void* (*)(const char*))ffdl_addr(gio, "g_file_new_for_path");
		_g_file_trash = (int (*)(void*, void*, void**))ffdl_addr(gio, "g_file_trash");
		if (_g_file_new_for_path == NULL || _g_file_trash == NULL) {
			e = "can't get g_file_new_for_path and g_file_trash from " LIBGIO_PATH;
			goto err;
		}

		if (gobject == FFDL_NULL) {
			if (FFDL_NULL == (gobject = ffdl_open(LIBGOBJECT_PATH, 0))) {
				if (FFDL_NULL == (gobject = ffdl_open(LIBGOBJECT_PATH_ALT, 0))) {
					e = "can't open " LIBGOBJECT_PATH;
					goto err;
				}
			}
		}

		_g_object_unref = (void (*)(void*))ffdl_addr(gobject, "g_object_unref");
		if (_g_object_unref == NULL) {
			e = "can't get g_object_unref from " LIBGOBJECT_PATH;
			goto err;
		}
		// ffdl_close(gio);
		// ffdl_close(gobject);
	}

	{
		void *gfile = _g_file_new_for_path(path);
		int r = _g_file_trash(gfile, NULL, NULL);
		_g_object_unref(gfile);
		if (r == 0) {
			e = "g_file_trash() returned error";
			goto err;
		}
	}
	return 0;

err:
	if (error != NULL)
		*error = e;
	return -1;
}

static inline int ffui_exec(const char *arg)
{
	const char *path = "/usr/bin/xdg-open";
	const char *argv[] = {
		"xdg-open", arg, NULL
	};
	ffps ps = ffps_exec(path, argv, (const char**)environ);
	if (ps == FFPS_NULL)
		return -1;
	ffps_close(ps);
	return 0;
}

#define ffui_openfolder1(path)  ffui_exec(path)

#endif
