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

import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.design.widget.Snackbar;
import android.support.design.widget.TabLayout;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.Html;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import eu.chainfire.suhide.AppAdapter.AppItem;

public class
    MainActivity
extends
        AppCompatActivity
implements
    AppFragment.OnListFragmentInteractionListener,
    MyApplication.OnStartupListener
{
    private SectionsPagerAdapter sectionsPagerAdapter;
    private ViewPager viewPager;
    private TabLayout tabLayout;

    private MyApplication application;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        application = ((MyApplication)getApplication());

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        setTitle(R.string.main_title);

        sectionsPagerAdapter = new SectionsPagerAdapter(getSupportFragmentManager());

        viewPager = (ViewPager) findViewById(R.id.container);
        viewPager.setAdapter(sectionsPagerAdapter);
        viewPager.setOffscreenPageLimit(3);
        viewPager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
            }

            @Override
            public void onPageSelected(int position) {
                invalidateOptionsMenu();
            }

            @Override
            public void onPageScrollStateChanged(int state) {
            }
        });

        tabLayout = (TabLayout) findViewById(R.id.tabs);
        tabLayout.setupWithViewPager(viewPager);

        application.setStartupListener(this);
        application.startup();
    }

    private void clearStartup() {
        application.setStartupListener(null);
        application.clearStartup();
    }

    @Override
    protected void onDestroy() {
        if (isFinishing()) {
            clearStartup();
        }
        super.onDestroy();
    }

    public class SectionsPagerAdapter extends FragmentPagerAdapter {
        public SectionsPagerAdapter(FragmentManager fm) {
            super(fm);
        }

        @Override
        public Fragment getItem(int position) {
            Fragment ret = null;
            if (position == 0) {
                ret = AppFragment.newInstance();
            } else if (position == 1) {
                ret = TextFragment.newInstance();
            }
            ret.setRetainInstance(true);
            return ret;
        }

        @Override
        public int getCount() {
            return 2;
        }

        @Override
        public CharSequence getPageTitle(int position) {
            switch (position) {
                case 0:
                    return "Apps";
                case 1:
                    return "About";
            }
            return null;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (viewPager.getCurrentItem() == 0) {
            MenuItem item = menu.add(R.string.btn_kill);
            item.setIcon(R.drawable.ic_kill);
            item.setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
            item.setOnMenuItemClickListener(new MenuItem.OnMenuItemClickListener() {
                @Override
                public boolean onMenuItemClick(MenuItem menuItem) {
                    performKill();
                    return true;
                }
            });
        }
        return true;
    }

    @Override
    protected void onUserLeaveHint() {
        finish();
        clearStartup();
        super.onUserLeaveHint();
    }

    @Override
    public void onBackPressed() {
        finish();
        clearStartup();
        super.onBackPressed();
    }

    @Override
    public void onListFragmentInteraction(AppItem item) {
        int state = item.nextState(true);
        String title = item.title;
        if (title == null) title = item.packageName;
        title = "<b>" + title + "</b>";
        int resId = 0;
        switch (state) {
            case AppItem.ROOT: resId = R.string.set_state_root; break;
            case AppItem.NO_ROOT: resId = R.string.set_state_no_root; break;
            case AppItem.HIDDEN: resId = R.string.set_state_hidden; break;
        }
        Snackbar.make(viewPager, Html.fromHtml(getString(resId, title)), Snackbar.LENGTH_SHORT).show();
        application.save();
    }

    @Override
    public void onStartupProgressMessage(String message) {
        ((TextView)findViewById(R.id.progress_text)).setText(message);
    }

    @Override
    public void onStartupErrorMessage(String message) {
        findViewById(R.id.progress_bar).setVisibility(View.GONE);
        findViewById(R.id.progress_error).setVisibility(View.VISIBLE);
        ((TextView)findViewById(R.id.progress_text)).setText(message);
    }

    @Override
    public void onStartupComplete() {
        findViewById(R.id.progress_container).setVisibility(View.GONE);
        findViewById(R.id.tabs).setVisibility(View.VISIBLE);
        findViewById(R.id.container).setVisibility(View.VISIBLE);
    }

    private class KillTask extends AsyncTask<Void, Void, Void> {
        private ProgressDialog progress = null;

        @Override
        protected void onPreExecute() {
            try {
                progress = new ProgressDialog(MainActivity.this);
                progress.setIndeterminate(true);
                progress.setCancelable(false);
                progress.setTitle(R.string.kill_title);
                progress.setMessage(getString(R.string.kill_message));
                progress.show();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        @Override
        protected Void doInBackground(Void... voids) {
            application.kill(true);
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            try {
                progress.dismiss();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private void performKill() {
        (new KillTask()).execute();
    }
}
