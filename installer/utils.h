/** phiola/Windows installer
Simon Zolin, 2024 */

/*
zip_unpack
env_path_add
*/

#include <ffpack/zipread.h>
#include <ffsys/file.h>
#include <ffsys/winreg.h>

#define ffsz_allocfmt_syserr(fmt, ...) \
	ffsz_allocfmt(fmt ": (%u) %s", __VA_ARGS__, fferr_last(), fferr_strptr(fferr_last()))

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
