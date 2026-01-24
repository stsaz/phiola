/** phiola/Android: utils
2024, Simon Zolin */

package com.github.stsaz.phiola;

class UtilNative {
	UtilNative(Phiola phi) {}

	native void storagePaths(String[] paths);
	native String trash(String trash_dir, String filepath);

	static class Files {
		String[] display_rows;
		String[] file_names;
		int n_directories;
	}
	native Files dirList(String path, int flags);
}
