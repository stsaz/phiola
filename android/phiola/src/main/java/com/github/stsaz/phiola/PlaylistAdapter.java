/** phiola/Android
2023, Simon Zolin */

package com.github.stsaz.phiola;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

class PlaylistViewHolder extends RecyclerView.ViewHolder
		implements View.OnClickListener, View.OnLongClickListener {

	final TextView text;

	interface Parent {
		int count();
		String display_line(int pos);
		void on_click(int i);
		void on_longclick(int i);
	}
	final Parent parent;

	PlaylistViewHolder(Parent parent, View itemView) {
		super(itemView);
		this.parent = parent;
		text = itemView.findViewById(R.id.list2_text);
		itemView.setClickable(true);
		itemView.setOnClickListener(this);
		itemView.setOnLongClickListener(this);
	}

	public void onClick(View v) {
		parent.on_click(getAdapterPosition());
	}

	public boolean onLongClick(View v) {
		parent.on_longclick(getAdapterPosition());
		return true;
	}
}

class PlaylistAdapter extends RecyclerView.Adapter<PlaylistViewHolder> {

	private static final String TAG = "phiola.PlaylistAdapter";
	private final Core core;
	private final Queue queue;
	private final LayoutInflater inflater;
	private final PlaylistViewHolder.Parent parent;

	PlaylistAdapter(Core core, Context ctx, PlaylistViewHolder.Parent parent) {
		this.core = core;
		queue = core.queue();
		inflater = LayoutInflater.from(ctx);
		this.parent = parent;
	}

	@NonNull
	public PlaylistViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
		View v = inflater.inflate(R.layout.list_row2, parent, false);
		return new PlaylistViewHolder(this.parent, v);
	}

	public void onBindViewHolder(@NonNull PlaylistViewHolder holder, int position) {
		holder.text.setText(parent.display_line(position));
	}

	public int getItemCount() { return parent.count(); }

	void on_change(int how, int pos) {
		core.dbglog(TAG, "on_change: %d %d", how, pos);
		if (pos < 0)
			notifyDataSetChanged();
		else if (how == QueueNotify.UPDATE)
			notifyItemChanged(pos);
		else if (how == QueueNotify.ADDED)
			notifyItemInserted(pos);
		else if (how == QueueNotify.REMOVED)
			notifyItemRemoved(pos);
	}
}
