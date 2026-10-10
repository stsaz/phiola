/** phiola/Windows installer
Simon Zolin, 2024 */

/*
ffmtx_open
dir_remove_r
zip_unpack
env_path_add env_path_remove
shell_ext_reg shell_ext_unreg
*/

#include <ffpack/zip-read.h>
#include <ffsys/error.h>
#include <ffsys/dir.h>
#include <ffsys/file.h>
#include <ffsys/winreg.h>
#include <ffsys/dirscan.h>
#include <ffbase/fntree.h>

typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char u_char;

#include <util/util.hpp>

#define ffsz_allocfmt_syserr(fmt, ...) \
	ffsz_allocfmt(fmt ": (%u) %s", __VA_ARGS__, fferr_last(), fferr_strptr(fferr_last()))


typedef HANDLE ffmtx;
#define FFMTX_NULL  NULL
#define FFMTX_CREATE  1

/** Create or open mutex object.
name: optional name
flags: FFMTX_CREATE or 0
Return FFMTX_NULL on error */
static inline ffmtx ffmtx_open(const char *name, uint flags)
{
	wchar_t wbuf[256], *wname = NULL;
	if (name != NULL
		&& NULL == (wname = ffsz_alloc_buf_utow(wbuf, FF_COUNT(wbuf), name)))
		return FFMTX_NULL;

	ffmtx h;
	if (flags & FFMTX_CREATE)
		h = CreateMutexW(NULL, 0, wname);
	else
		h = OpenMutexW(SYNCHRONIZE, 0, wname);

	if (wname != wbuf)
		ffmem_free(wname);
	return h;
}

static inline int ffmtx_wait(ffmtx h, uint time_ms)
{
	int r = WaitForSingleObject(h, time_ms);
	if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED)
		r = 0;
	else if (r == WAIT_TIMEOUT)
		SetLastError(WSAETIMEDOUT);
	return r;
}

static inline void ffmtx_close(ffmtx h)
{
	if (h != FFMTX_NULL)
		CloseHandle(h);
}


typedef int (*job_t)(int r, const char *path);

/** Scan the directory: delete files immediately, add subdirs to the tree.
Reparse points (junction/symlink) are removed as links.
limit: remaining N of entries allowed to process */
static int _dir_remove_scan(ffvec *fpath, fntree_block **blk, int *limit, job_t job)
{
	int r = 0;
	ffdirscan ds = {};
	ffstr path = fntree_path(*blk);
	if (path.len + 1 + 255 + 1 > fpath->cap)
		return -1;
	if (ffdirscan_open(&ds, path.ptr, FFDIRSCAN_NOSORT))
		return -1;

	*limit -= ffdirscan_count(&ds);
	if (*limit < 0) {
		r = -1;
		goto end;
	}

	fpath->len = 0;
	ffvec_catstr(fpath, &path);
	ffvec_catchar(fpath, '\\');

	const char *name;
	while ((name = ffdirscan_next(&ds))) {
		fpath->len = path.len + 1;
		ffvec_catz(fpath, name);
		ffvec_catchar(fpath, '\0');
		fpath->len--;
		const char *fn = (char*)fpath->ptr;

		fffileinfo fi;
		if (fffile_info_path(fn, &fi))
			continue;
		uint attr = fffileinfo_attr(&fi);

		if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
			if (fffile_isdir(attr)) {
				r |= job(ffdir_remove(fn), fn);
			} else {
				r |= job(fffile_remove(fn), fn);
			}
			continue;
		}

		if (fffile_isdir(attr)) {
			fntree_entry *e;
			if (!(e = fntree_addz(blk, name, 0)))
				continue;
			fntree_attach(e, fntree_create(ffvec_str(fpath)));
		} else {
			r |= job(fffile_remove(fn), fn);
		}
	}

end:
	ffdirscan_close(&ds);
	return r;
}

/** Recursively delete the directory tree.
Stop if more than 'limit' total entries are found (Note: some files may still be deleted). */
static int dir_remove_r(const char *dir, uint limit, job_t job)
{
	fffileinfo fi;
	if (fffile_info_path(dir, &fi))
		return -1;
	if (fffileinfo_attr(&fi) & FILE_ATTRIBUTE_REPARSE_POINT) {
		return job(ffdir_remove(dir), dir); // If the root is a reparse point: remove the link only
	}

	int r = 0;
	fntree_block *root = fntree_create(FFSTR_Z(dir));
	fntree_block *blk = root;
	fntree_cursor cur = {};
	fntree_entry *e = NULL;
	ffvec fpath = {};
	ffvec_alloc(&fpath, 4096, 1);

	// Scan; delete files; build the directory tree
	for (;;) {
		r |= _dir_remove_scan(&fpath, &blk, (int*)&limit, job);
		if ((int)limit < 0) {
			r = -1;
			goto end;
		}
		if (e)
			e->children = blk;

		if (!(e = fntree_cur_next_r(&cur, &blk)))
			break;
		blk = e->children;
	}

	// Delete directories in post-order (deepest first, root last)
	blk = root;
	ffmem_zero_obj(&cur);
	for (;;) {
		if (!(blk = _fntr_blk_next_r_post(&cur, blk)))
			break;
		r |= job(ffdir_remove(fntree_path(blk).ptr), fntree_path(blk).ptr);
	}

end:
	fntree_free_all(root);
	return r;
}

/** Unpack zip archive to the specified directory.
backup_files: eg "dir/name1.old\0dir/name2.old\0"
	These are NOT reverted automatically in case of error.
Return error message;
	(char*)-1 if zip data is corrupted. */
static inline char* zip_unpack(ffstr pkg, ffstr dir, uint upgrade, ffvec *backup_files)
{
	char *e = (char*)-1;

	ffzipread rzip = {};
	ffzipread_open(&rzip, pkg.len);

	struct unzip_ent {
		uint64 off, comp_size;
	};
	ffvec index = {}; // struct unzip_ent[]

	ffstr in = {}, body;
	uint64 off = 0;
	uint cur = 0;
	fffd f = FFFILE_NULL;

	ffvec fn = {}, fn_old = {};
	ffvec_alloc(&fn, dir.len + 1 + 255 + 1, 1);
	ffvec_catstr(&fn, &dir);
	ffvec_catchar(&fn, '\\');
	if (upgrade) {
		ffvec_alloc(&fn_old, dir.len + 1 + 255 + 1, 1);
		ffvec_catstr(&fn_old, &dir);
		ffvec_catchar(&fn_old, '\\');
	}

	for (;;) {

		int r = ffzipread_process(&rzip, &in, &body);
		switch ((enum FFZIPREAD_R)r) {

		case FFZIPREAD_SEEK:
			off = ffzipread_offset(&rzip);
			// fallthrough
		case FFZIPREAD_MORE:
			in = pkg;
			ffstr_shift(&in, off);
			if (!in.len) {
				goto end;
			}
			off += in.len;
			break;

		case FFZIPREAD_FILEINFO: {
			const ffzipread_fileinfo_t *zi = ffzipread_fileinfo(&rzip);
			struct unzip_ent *ent = ffvec_pushT(&index, struct unzip_ent);
			ent->off = zi->hdr_offset;
			ent->comp_size = zi->compressed_size;
			break;
		}

		case FFZIPREAD_FILEDONE:
		case FFZIPREAD_DONE: {
			if (cur == index.len) {
				e = NULL;
				goto end;
			}
			const struct unzip_ent *ent = ffslice_itemT(&index, cur, struct unzip_ent);
			cur++;
			ffzipread_fileread(&rzip, ent->off, ent->comp_size);
			break;
		}

		case FFZIPREAD_FILEHEADER: {
			if (fffile_close(f)) {
				f = FFFILE_NULL;
				e = ffsz_allocfmt_syserr("file write: %s", fn.ptr);
				goto end;
			}
			f = FFFILE_NULL;

			const ffzipread_fileinfo_t *zi = ffzipread_fileinfo(&rzip);
			fn.len = dir.len + 1;
			if (zi->name.len > 255)
				goto end;
			ffvec_catstr(&fn, &zi->name);
			ffvec_catchar(&fn, '\0');
			fn.len--;
			char *name = (char*)fn.ptr;

			if (name[fn.len - 1] == '/') {
				name[fn.len - 1] = '\0';
				if (ffdir_make(name)
					&& !(upgrade && fferr_exist(fferr_last()))) {
					e = ffsz_allocfmt_syserr("directory make: %s", name);
					goto end;
				}
				continue;
			}

			if (FFFILE_NULL == (f = fffile_open(name, FFFILE_CREATENEW | FFFILE_WRITEONLY))) {
				if (fferr_exist(fferr_last()) && upgrade) {
					fn_old.len = dir.len + 1;
					if (!ffvec_catf(&fn_old, "%S.old%Z", &zi->name))
						goto end;
					const char *name_old = (char*)fn_old.ptr;
					ffvec_addT(backup_files, name_old, ffsz_len(name_old) + 1, char);
					if (fffile_rename(name, name_old)) {
						e = ffsz_allocfmt_syserr("file rename: %s", name);
						goto end;
					}
					f = fffile_open(name, FFFILE_CREATENEW | FFFILE_WRITEONLY);
				}
				if (f == FFFILE_NULL) {
					e = ffsz_allocfmt_syserr("file create: %s", name);
					goto end;
				}
			}
			break;
		}

		case FFZIPREAD_DATA:
			if (body.len != fffile_write(f, body.ptr, body.len)) {
				e = ffsz_allocfmt_syserr("file write: %s", fn.ptr);
				goto end;
			}
			break;

		case FFZIPREAD_WARNING:
		case FFZIPREAD_ERROR:
			goto end;
		}
	}

end:
	ffzipread_close(&rzip);
	ffvec_free(&index);
	if (fffile_close(f)) {
		e = ffsz_allocfmt_syserr("file write: %s", fn.ptr);
	}
	ffvec_free(&fn);
	ffvec_free(&fn_old);
	return e;
}

/** Select the region with the specified component from a ';'-delimited string. */
static ffstr env_path_find(ffstr s, ffstr find)
{
	ssize_t i;
	size_t start, end;
	if ((i = ffstr_ifindstr(&s, &find)) < 0)
		goto err;

	start = i;
	if (i == 0)
		; // [find]
	else if (s.ptr[i - 1] == ';')
		start--; // ...[;find]
	else
		goto err; // ?find

	end = i + find.len;
	if (end == s.len) {
		; // ...[;find]
	} else if (s.ptr[end] == ';') {
		if (i == 0)
			end++; // [find;]...
		else
			; // ...[;find];...
	} else {
		goto err; // find?
	}

	return FFSTR_N(s.ptr + start, end - start);

err:
	return (ffstr){};
}

/** Add path to the user's PATH environment variable. */
static inline int env_path_add(ffstr path)
{
	ffwinreg k = FFWINREG_NULL;
	ffstr val = {};
	ffvec d = {};
	int r = -1;

	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, "Environment", FFWINREG_READWRITE)))
		goto end;

	if (!ffwinreg_readstr(k, "PATH", &val)) {
		if (env_path_find(val, path).len) {
			r = 0;
			goto end; // Path already exists
		}
		ffvec_set3(&d, val.ptr, val.len, val.len);
		ffstr_null(&val);
	}

	ffvec_grow(&d, path.len + 1, 1);
	if (d.len && ((char*)d.ptr)[d.len - 1] != ';')
		ffvec_catchar(&d, ';');
	ffvec_catstr(&d, &path);

	if (ffwinreg_writestr(k, "PATH", (char*)d.ptr, d.len))
		goto end;

	r = 0;

end:
	ffwinreg_close(k);
	ffvec_free(&d);
	ffmem_free(val.ptr);
	return r;
}

/** Remove path from the user's PATH environment variable.
Return 0 on success. */
static inline int env_path_remove(ffstr path, job_t job)
{
	ffwinreg k;
	ffstr s = {}, sel;
	int rc = -1;
	ffvec buf = {};

	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, "Environment", FFWINREG_READWRITE)))
		goto done;

	if (ffwinreg_readstr(k, "PATH", &s))
		goto done; // no PATH or not a string

	sel = env_path_find(s, path);
	if (!sel.len)
		goto done; // 'path' is not in PATH

	ffmem_move(sel.ptr, sel.ptr + sel.len, s.ptr + s.len - (sel.ptr + sel.len));
	s.len -= sel.len;
	ffvec_addfmt(&buf, "Environment\\PATH=%S%Z", &s);
	rc = job(ffwinreg_writestr(k, "PATH", (char*)s.ptr, s.len), (char*)buf.ptr);
	goto fin;

done:
	rc = 0;
fin:
	ffwinreg_close(k);
	ffmem_free(s.ptr);
	ffvec_free(&buf);
	return rc;
}

static inline int ffwinreg_open_writestr(HKEY hk, const char *path, const char *name, const char *val, ffsize len)
{
	ffwinreg k;
	if (FFWINREG_NULL == (k = ffwinreg_open(hk, path, FFWINREG_CREATE | FFWINREG_WRITEONLY)))
		return -1;
	int r = ffwinreg_writestr(k, name, val, len);
	ffwinreg_close(k);
	return r;
}

static inline int ffwinreg_open_writez(HKEY hk, const char *path, const char *name, const char *valz) {
	return ffwinreg_open_writestr(hk, path, name, valz, ffsz_len(valz));
}

/** Read string value from registry.
val: output string.  Free with ffmem_free().
Return 0 on success */
static inline int ffwinreg_open_readstr(HKEY hk, const char *subkey, const char *name, ffstr *val)
{
	int r = -1;
	ffwinreg k;
	ffwinreg_val v = {};
	if (FFWINREG_NULL == (k = ffwinreg_open(hk, subkey, FFWINREG_READONLY)))
		goto end;
	if (1 != ffwinreg_read(k, name, &v)
		|| !ffwinreg_isstr(v.type))
		goto end;
	r = 0;

end:
	ffwinreg_close(k);
	ffstr_set(val, v.data, v.datalen);
	return r;
}

static inline int ffwinreg_open_del(HKEY hk, const char *path, const char *key, const char *val)
{
	ffwinreg k;
	if (FFWINREG_NULL == (k = ffwinreg_open(hk, path, FFWINREG_WRITEONLY)))
		return -1;
	int r = ffwinreg_del(k, key, val);
	ffwinreg_close(k);
	return r;
}

static inline char* ffvec_sz(ffvec *v)
{
	if (v->len < v->cap && ((char*)v->ptr)[v->len] != '\0')
		((char*)v->ptr)[v->len] = '\0';
	return (char*)v->ptr;
}

/** Register phiola for the specified file extensions, for the current user only.
The tree to create:
	SOFTWARE\
		phiola\capabilities\
			ApplicationName = "phiola"
			FileAssociations\
				.AAC = "phiola.AAC"
				...
		RegisteredApplications\
			phiola = "Software\phiola\capabilities"
		Classes\
			Applications\phiola-gui.exe\shell\
				open\
					= "Open with phiola"
					command\ = "<exe>" "%1"
				enqueue\
					= "Enqueue in phiola"
					command\ = "<exe>" -add "%1"
			phiola.AAC\shell\
				open\
					= "Open with phiola"
					command\ = "<exe>" "%1"
				enqueue\
					= "Enqueue in phiola"
					command\ = "<exe>" -add "%1"
			...
Return 0 on success. */
static inline int shell_ext_reg(const char *app_name, const char *exe
	, const char *cmd_open_label, const char *cmd_open
	, const char *cmd2_name, const char *cmd2_label, const char *cmd2
	, const char *exts, uint ext_sz, uint exts_n)
{
	int r = 0;
	ffwinreg k = FFWINREG_NULL;
	ffvec buf = {};
	ffvec_alloc(&buf, FFS_LEN("Software\\Classes\\Applications\\...\\shell\\open\\command")
		+ ffsz_len(app_name) + ffsz_len(exe) + ((cmd2_name) ? ffsz_len(cmd2_name) : 0) + ext_sz + 1, 1);

	ffvec_catf(&buf, "Software\\Classes\\Applications\\%s\\shell", exe);
	uint base = buf.len;

	ffvec_catz(&buf, "\\open");
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd_open_label);
	ffvec_catz(&buf, "\\command");
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd_open);

	if (cmd2_name) {
		buf.len = base;
		ffvec_catf(&buf, "\\%s", cmd2_name);
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd2_label);
		ffvec_catz(&buf, "\\command");
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd2);
	}

	buf.len = 0;
	ffvec_catf(&buf, "Software\\%s\\capabilities", app_name);
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "ApplicationName", app_name);
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, "Software\\RegisteredApplications", app_name, ffvec_sz(&buf));
	ffvec_catz(&buf, "\\FileAssociations");
	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, ffvec_sz(&buf), FFWINREG_CREATE | FFWINREG_WRITEONLY))) {
		r = -1;
		goto end;
	}

	for (uint i = 0;  i < exts_n;  i++) {
		const char *ext = exts + i * ext_sz;
		buf.len = 0;
		ffvec_catf(&buf, "Software\\Classes\\%s.%s\\shell", app_name, ext);
		//                              ^x ^
		//                            ^val ^
		ffstr x = FFSTR_N((char*)buf.ptr + FFS_LEN("Software\\Classes\\") + ffsz_len(app_name), 1 + ffsz_len(ext));
		ffs_upper(x.ptr, x.len, x.ptr, x.len);
		base = buf.len;

		ffvec_catz(&buf, "\\open");
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd_open_label);
		ffvec_catz(&buf, "\\command");
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd_open);

		if (cmd2_name) {
			buf.len = base;
			ffvec_catf(&buf, "\\%s", cmd2_name);
			r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd2_label);
			ffvec_catz(&buf, "\\command");
			r |= ffwinreg_open_writez(HKEY_CURRENT_USER, ffvec_sz(&buf), "", cmd2);
		}

		ffstr val = FFSTR_N((char*)buf.ptr + FFS_LEN("Software\\Classes\\"), ffsz_len(app_name) + x.len);
		x.ptr[x.len] = '\0';
		r |= ffwinreg_writestr(k, x.ptr, val.ptr, val.len); // ".EXT = phiola.EXT"
	}

end:
	ffwinreg_close(k);
	ffvec_free(&buf);
	return r;
}

/** Delete phiola registry keys */
static int shell_ext_unreg(const char *app_name, const char *exe
	, const char *exts, uint ext_sz, uint exts_n
	, job_t job)
{
	int r = 0, rc;
	ffvec buf = {};
	ffvec_alloc(&buf, FFS_LEN("Software\\RegisteredApplications\\") + ffsz_len(exe) + ffsz_len(app_name) + 1 + ext_sz, 1);

	ffvec_catf(&buf, "Software\\RegisteredApplications\\%s", app_name);
	r |= job(ffwinreg_open_del(HKEY_CURRENT_USER, "Software\\RegisteredApplications", "", app_name), ffvec_sz(&buf));

	buf.len = 0;
	ffvec_catf(&buf, "Software\\%s", app_name);
	rc = ffwinreg_deltree(HKEY_CURRENT_USER, ffvec_sz(&buf));
	ffvec_catchar(&buf, '\\');
	r |= job(rc, ffvec_sz(&buf));

	buf.len = 0;
	ffvec_catf(&buf, "Software\\Classes\\Applications\\%s", exe);
	rc = ffwinreg_deltree(HKEY_CURRENT_USER, ffvec_sz(&buf));
	ffvec_catchar(&buf, '\\');
	r |= job(rc, ffvec_sz(&buf));

	for (uint i = 0;  i < exts_n;  i++) {
		const char *ext = exts + i * ext_sz;
		buf.len = 0;
		ffvec_catf(&buf, "Software\\Classes\\%s.%s", app_name, ext);
		rc = ffwinreg_deltree(HKEY_CURRENT_USER, ffvec_sz(&buf));
		ffvec_catchar(&buf, '\\');
		r |= job(rc, ffvec_sz(&buf));
	}

	ffvec_free(&buf);
	return r;
}
