/** phiola/Android: UI: draw all rows in RecyclerView
2026, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Context;
import android.util.AttributeSet;

import androidx.recyclerview.widget.RecyclerView;

public class FullRecyclerView extends RecyclerView {
	public FullRecyclerView(Context c) { super(c); }
	public FullRecyclerView(Context c, AttributeSet a) { super(c, a); }
	public FullRecyclerView(Context c, AttributeSet a, int d) { super(c, a, d); }

	@Override
	protected void onMeasure(int widthSpec, int heightSpec) {
		super.onMeasure(widthSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
	}
}
