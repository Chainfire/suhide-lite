/*
 * Copyright (C) 2017 Jorrit "Chainfire" Jongma & CCMT
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package eu.chainfire.suhide;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import eu.chainfire.suhide.AppFragment.OnListFragmentInteractionListener;

public class AppAdapter extends RecyclerView.Adapter<AppAdapter.ViewHolder> {
    private final List<AppItem> items;
    private OnListFragmentInteractionListener listener;

    public AppAdapter() {
        this.items = new ArrayList<AppItem>();
        this.listener = null;
    }

    public void setOnListFragmentInteractionListener(OnListFragmentInteractionListener listener) {
        this.listener = listener;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.fragment_app, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(final ViewHolder holder, int position) {
        holder.appItem = items.get(position);
        holder.ivIcon.setImageDrawable(holder.appItem.icon);
        if (holder.appItem.title == null) {
            holder.tvTitle.setText(holder.appItem.packageName);
            holder.tvPackageName.setText(String.valueOf(holder.appItem.uid));
        } else {
            holder.tvTitle.setText(holder.appItem.title);
            holder.tvPackageName.setText(String.valueOf(holder.appItem.uid) + " " + holder.appItem.packageName);
        }
        switch (holder.appItem.state) {
            case AppItem.ROOT: holder.ivState.setImageResource(R.drawable.ic_root); break;
            case AppItem.NO_ROOT: holder.ivState.setImageResource(R.drawable.ic_no_root); break;
            case AppItem.HIDDEN: holder.ivState.setImageResource(R.drawable.ic_hidden); break;
        }
        holder.view.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (null != listener) {
                    listener.onListFragmentInteraction(holder.appItem);
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    public AppItem getItem(int position) {
        return items.get(position);
    }

    public class ViewHolder extends RecyclerView.ViewHolder {
        public final View view;
        public final ImageView ivIcon;
        public final TextView tvTitle;
        public final TextView tvPackageName;
        public final ImageView ivState;
        public AppItem appItem;

        public ViewHolder(View view) {
            super(view);
            this.view = view;
            ivIcon = (ImageView) view.findViewById(R.id.icon);
            tvTitle = (TextView) view.findViewById(R.id.title);
            tvPackageName = (TextView) view.findViewById(R.id.packageName);
            ivState = (ImageView) view.findViewById(R.id.state);
        }

        @Override
        public String toString() {
            return super.toString() + " '" + appItem.packageName + "'";
        }
    }

    private static String getLabel(PackageManager pm, ApplicationInfo info) {
        try {
            return info.loadLabel(pm).toString();
        } catch (Exception e) {
            return null;
        }
    }

    private static Drawable getIcon(PackageManager pm, ApplicationInfo info) {
        try {
            return info.loadIcon(pm);
        } catch (Exception e) {
            return null;
        }
    }

    public void loadAppItems(Context context) {
        List<AppItem> results = new ArrayList<AppItem>();
        PackageManager pm = context.getPackageManager();
        List<ApplicationInfo> applications = pm.getInstalledApplications(PackageManager.GET_META_DATA);
        for (ApplicationInfo info : applications) {
            if (info.uid >= 10000) {
                results.add(new AppItem(
                    getLabel(pm, info),
                    info.uid,
                    info.packageName,
                    getIcon(pm, info),
                    AppItem.ROOT
                ));
            }
        }
        Collections.sort(results, new Comparator<AppItem>() {
            @Override
            public int compare(AppItem a, AppItem b) {
                if ((a.title == null) && (b.title == null)) {
                    return a.packageName.compareToIgnoreCase(b.packageName);
                } else if ((a.title == null) && (b.title != null)) {
                    return 1;
                } else if ((a.title != null) && (b.title == null)) {
                    return -1;
                } else {
                    return a.title.compareToIgnoreCase(b.title);
                }
            }
        });
        items.clear();
        for (AppItem item : results) {
            items.add(item);
        }
    }

    public class AppItem {
        public static final int ROOT = 0;
        public static final int NO_ROOT = 1;
        public static final int HIDDEN = 2;

        public final String title;
        public final int uid;
        public final String packageName;
        public final Drawable icon;
        public int state;

        public AppItem(String title, int uid, String packageName, Drawable icon, int state) {
            if ((title != null) && (packageName != null) && (title.equals(packageName))) {
                title = null;
            }
            this.title = title;
            this.uid = uid;
            this.packageName = packageName;
            this.icon = icon;
            this.state = state;
        }

        private void changed() {
            int index = -1;
            for (int i = 0; i < items.size(); i++) {
                if (items.get(i) == this) {
                    index = i;
                    break;
                }
            }
            if (index == -1) {
                // this kills the ripple
                notifyDataSetChanged();
            } else {
                notifyItemChanged(index);
            }
        }

        public int nextState(boolean uid) {
            if (state < HIDDEN) {
                state++;
            } else {
                state = ROOT;
            }
            changed();
            if (uid) {
                for (int i = 0; i < items.size(); i++) {
                    if (items.get(i).uid == this.uid) {
                        items.get(i).setState(state);
                    }
                }
            }
            return state;
        }

        public void setState(int state) {
            this.state = state;
            changed();
        }
    }
}
