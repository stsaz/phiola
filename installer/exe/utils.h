/** phiola/Windows installer
Simon Zolin, 2024 */

/*
zip_unpack
env_path_add
shell_ext_reg
*/

#include <ffpack/zip-read.h>
#include <ffsys/error.h>
#include <ffsys/dir.h>
#include <ffsys/file.h>
#include <ffsys/winreg.h>

typedef long long int64;
typedef unsigned long long uint64;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char u_char;

#include <util/util.hpp>

#define ffsz_allocfmt_syserr(fmt, ...) \
	ffsz_allocfmt(fmt ": (%u) %s", __VA_ARGS__, fferr_last(), fferr_strptr(fferr_last()))

static xxvec vlog;
static int job(int r, const char *path)
{
	vlog.add_f("%s %s\r\n"
		, (!r) ? "OK " : "ERR", path);
	return r;
}

/** Unpack zip archive to the specified directory.
Return error message;
	(char*)-1 if zip data is corrupted. */
static inline char* zip_unpack(ffstr pkg, ffstr dir)
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

	ffvec fn = {};
	ffvec_addstr(&fn, &dir);
	ffvec_addchar(&fn, '\\');

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
			ffvec_addstr(&fn, &zi->name);
			ffvec_addchar(&fn, '\0');
			fn.len--;
			char *name = (char*)fn.ptr;

			if (name[fn.len - 1] == '/') {
				name[fn.len - 1] = '\0';
				if (ffdir_make(name)) {
					e = ffsz_allocfmt_syserr("directory make: %s", name);
					goto end;
				}
				continue;
			}

			if (FFFILE_NULL == (f = fffile_open(name, FFFILE_CREATENEW | FFFILE_WRITEONLY))) {
				e = ffsz_allocfmt_syserr("file create: %s", name);
				goto end;
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
	return e;
}

/** Add path to the user's PATH environment variable. */
static inline int env_path_add(ffstr path)
{
	ffwinreg k = FFWINREG_NULL;
	ffwinreg_val val = {};
	ffvec path_data = {};
	int r, rc = -1;

	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, "Environment", FFWINREG_READWRITE)))
		goto end;

	if (1 == ffwinreg_read(k, "PATH", &val)) {
		if (!ffwinreg_isstr(val.type))
			goto end; // PATH must be of STRING type

		ffvec_set3(&path_data, val.data, val.datalen, val.datalen);
		if (ffstr_ifindstr((ffstr*)&path_data, &path) >= 0) {
			rc = 0;
			goto end; // Path already exists
		}

	} else {
		val.type = REG_SZ;
	}

	if (path_data.len && ((char*)path_data.ptr)[path_data.len - 1] != ';')
		ffvec_addchar(&path_data, ';');
	ffvec_addstr(&path_data, &path);
	ffvec_addchar(&path_data, ';');

	val.data = (char*)path_data.ptr;
	val.datalen = path_data.len;
	if (ffwinreg_write(k, "PATH", &val))
		goto end;

	rc = 0;

end:
	ffwinreg_close(k);
	ffvec_free(&path_data);
	return rc;
}

/** Remove path from the user's PATH environment variable.
Return 0 on success. */
static inline int env_path_remove(ffstr path)
{
	ffwinreg k;
	ffwinreg_val val = {};
	ffssize i;
	ffsize start, end;
	xxstr s;
	int rc = -1;

	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, "Environment", FFWINREG_READWRITE)))
		goto done;

	if (1 != ffwinreg_read(k, "PATH", &val)
		|| !ffwinreg_isstr(val.type))
		goto done; // no PATH or not a string

	s.set(val.data, val.datalen);
	if ((i = s.find_str_i(path)) < 0)
		goto done; // 'path' is not in PATH

	start = i;
	if (i == 0)
		; // [phiola;]...
	else if (i > 1 && s.ptr[i - 1] == ';')
		start--; // ...[;phiola]
	else
		goto fin; // ?phiola

	end = i + path.len;
	if (end == s.len) {
		; // ...[;phiola]
	} else if (s.ptr[end] == ';') {
		if (i == 0)
			end++; // [phiola;]...
		else
			; // ...[;phiola];...
	} else {
		goto fin; // phiola?
	}
	ffmem_move(s.ptr + start, s.ptr + end, s.len - end);
	s.len -= end - start;
	val.datalen = s.len;
	rc = job(ffwinreg_write(k, "PATH", &val), xxvec().add_f("Environment\\PATH=%S", &s).strz());
	goto fin;

done:
	rc = 0;
fin:
	ffwinreg_close(k);
	ffmem_free(val.data);
	return rc;
}

static inline int ffwinreg_open_writestr(HKEY hk, const char *path, const char *name, const char *val, ffsize len)
{
	ffwinreg k;
	if (FFWINREG_NULL == (k = ffwinreg_open(hk, path, FFWINREG_CREATE | FFWINREG_WRITEONLY)))
		return -1;
	ffwinreg_writestr(k, name, val, len);
	ffwinreg_close(k);
	return 0;
}

static inline int ffwinreg_open_writez(HKEY hk, const char *path, const char *name, const char *valz) {
	return ffwinreg_open_writestr(hk, path, name, valz, ffsz_len(valz));
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
static inline int shell_ext_reg(const char *exe
	, const char *cmd_open_label, const char *cmd_open
	, const char *cmd_add_label, const char *cmd_add
	, const char *exts, uint ext_sz, uint exts_n)
{
	int r = 0;
	ffwinreg k;
	xxvec buf;
	buf.alloc<char>(FFS_LEN("Software\\Classes\\Applications\\...\\shell\\enqueue\\command") + ffsz_len(exe) + ext_sz + 1);

	buf.cat_f("Software\\Classes\\Applications\\%s\\shell", exe);
	uint base = buf.len;

	buf.cat("\\open");
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.sz(), "", cmd_open_label);
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.cat("\\command").sz(), "", cmd_open);

	buf.len = base;
	buf.cat("\\enqueue");
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.sz(), "", cmd_add_label);
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.cat("\\command").sz(), "", cmd_add);

	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, "Software\\RegisteredApplications", "phiola", "Software\\phiola\\capabilities");
	r |= ffwinreg_open_writez(HKEY_CURRENT_USER, "Software\\phiola\\capabilities", "ApplicationName", "phiola");

	if (FFWINREG_NULL == (k = ffwinreg_open(HKEY_CURRENT_USER, "Software\\phiola\\capabilities\\FileAssociations", FFWINREG_CREATE | FFWINREG_WRITEONLY)))
		return -1;

	for (uint i = 0;  i < exts_n;  i++) {
		const char *ext = exts + i * ext_sz;
		buf.len = 0;
		buf.cat_f("Software\\Classes\\phiola.%s\\shell", ext);
		//                                  ^x ^
		//                            ^ val    ^
		xxstr x(buf.sz() + FFS_LEN("Software\\Classes\\phiola"), 1 + ffsz_len(ext));
		ffs_upper(x.ptr, x.len, x.ptr, x.len);
		base = buf.len;

		buf.cat("\\open");
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.sz(), "", cmd_open_label);
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.cat("\\command").sz(), "", cmd_open);

		buf.len = base;
		buf.cat("\\enqueue");
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.sz(), "", cmd_add_label);
		r |= ffwinreg_open_writez(HKEY_CURRENT_USER, buf.cat("\\command").sz(), "", cmd_add);

		xxstr val(buf.sz() + FFS_LEN("Software\\Classes\\"), FFS_LEN("phiola") + x.len);
		x.ptr[x.len] = '\0';
		r |= ffwinreg_writestr(k, x.ptr, val.ptr, val.len); // ".EXT = phiola.EXT"
	}

	ffwinreg_close(k);
	return r;
}

/** Delete phiola registry keys */
static int shell_ext_unreg(const char *exe
	, const char *exts, uint ext_sz, uint exts_n)
{
	int r = 0;
	r |= job(ffwinreg_open_del(HKEY_CURRENT_USER, "Software\\RegisteredApplications", "", "phiola"), "Software\\RegisteredApplications\\phiola");
	r |= job(ffwinreg_deltree(HKEY_CURRENT_USER, "Software\\phiola"), "Software\\phiola\\");

	xxvec buf;
	buf.alloc<char>(FFS_LEN("Software\\Classes\\Applications\\") + ffsz_len(exe) + FFS_LEN("phiola.") + ext_sz);

	buf.cat_f("Software\\Classes\\Applications\\%s", exe);
	int rc = ffwinreg_deltree(HKEY_CURRENT_USER, buf.sz());
	r |= job(rc, buf.cat_f("\\").sz());

	for (uint i = 0;  i < exts_n;  i++) {
		const char *ext = exts + i * ext_sz;
		buf.len = 0;
		buf.cat_f("Software\\Classes\\phiola.%s", ext);
		rc = ffwinreg_deltree(HKEY_CURRENT_USER, buf.sz());
		r |= job(rc, buf.cat_f("\\").sz());
	}

	return r;
}
