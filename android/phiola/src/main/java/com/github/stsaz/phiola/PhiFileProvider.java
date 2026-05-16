/** phiola/Android: class for passing file descriptors from phiola to other apps
2026, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Context;
import android.net.Uri;

import java.io.File;

public class PhiFileProvider extends androidx.core.content.FileProvider {
	Uri uri(Context ctx, String file_name) {
		return getUriForFile(ctx, "com.github.stsaz.phiola.fileprovider", new File(file_name));
	}
}
