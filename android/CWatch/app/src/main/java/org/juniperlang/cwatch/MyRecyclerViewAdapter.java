package org.juniperlang.cwatch;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;

public class MyRecyclerViewAdapter extends RecyclerView.Adapter<MyRecyclerViewAdapter.ViewHolder> {
    private List<String> mData;
    private LayoutInflater mInflater;
    private ItemClickListener mClickListener;

    // data is passed into the constructor
    MyRecyclerViewAdapter(Context context, List<String> data) {
        this.mInflater = LayoutInflater.from(context);
        this.mData = data;
    }

    MyRecyclerViewAdapter(Context context) {
        this.mInflater = LayoutInflater.from(context);
        this.mData = new ArrayList<String>();
    }

    // inflates the row layout from xml when needed
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = mInflater.inflate(R.layout.recyclerview_row, parent, false);
        return new ViewHolder(view);
    }

    // binds the data to the TextView in each row
    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        String animal = mData.get(position);
        holder.myTextView.setText(animal);
    }

    // total number of rows
    @Override
    public int getItemCount() {
        return mData.size();
    }

    // stores and recycles views as they are scrolled off screen
    public class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        TextView myTextView;

        ViewHolder(View itemView) {
            super(itemView);
            myTextView = itemView.findViewById(R.id.tvBLEName);
            itemView.setOnClickListener(this);
        }

        @Override
        public void onClick(View view) {
            if (mClickListener != null) mClickListener.onItemClick(view, getAdapterPosition());
        }
    }

    // convenience method for getting data at click position
    String getItem(int id) {
        return mData.get(id);
    }

    // allows clicks events to be caught
    void setClickListener(ItemClickListener itemClickListener) {
        this.mClickListener = itemClickListener;
    }

    // parent activity will implement this method to respond to click events
    public interface ItemClickListener {
        void onItemClick(View view, int position);
    }

    public void insertSingleItem(String item) {
        mData.add(item);
        notifyItemInserted(mData.size() - 1);
    }

    public void insertSingleItem(int insertIndex, String item) {
        mData.add(insertIndex, item);
        notifyItemInserted(insertIndex);
    }

    public void insertMultipleItems(int insertIndex, List<String> items) {
        mData.addAll(insertIndex, items);
        notifyItemRangeInserted(insertIndex, items.size());
    }

    public void removeSingleItem(int removeIndex) {
        mData.remove(removeIndex);
        notifyItemRemoved(removeIndex);
    }

    // startIndex is inclusive
    // endIndex is exclusive
    public void removeMultipleItems(int startIndex, int endIndex) {
        int count = endIndex - startIndex;
        mData.subList(startIndex, endIndex).clear();
        notifyItemRangeRemoved(startIndex, count);
    }

    public void removeAllItems() {
        mData.clear();
        notifyDataSetChanged();
    }

    public void replaceOldListWithNewList(List<String> newList) {
        // clear old list
        mData.clear();
        mData.addAll(newList);

        // notify adapter
        notifyDataSetChanged();
    }

    public void updateSingleItem(int updateIndex, String newValue) {
        mData.set(updateIndex, newValue);
        notifyItemChanged(updateIndex);
    }

    private void moveSingleItem(int fromPosition, int toPosition) {
        // update data array
        String item = mData.get(fromPosition);
        mData.remove(fromPosition);
        mData.add(toPosition, item);

        // notify adapter
        notifyItemMoved(fromPosition, toPosition);
    }
}
