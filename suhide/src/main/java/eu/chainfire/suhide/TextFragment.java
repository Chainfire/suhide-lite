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

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class TextFragment extends Fragment {
    public TextFragment() {
    }

    public static TextFragment newInstance() {
        TextFragment fragment = new TextFragment();
        return fragment;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_main, container, false);
        TextView textView = (TextView) rootView.findViewById(R.id.about);

        String version = "?.??";
        PackageManager pm = getContext().getPackageManager();
        if (pm != null) {
            try {
                PackageInfo pi = pm.getPackageInfo(getContext().getPackageName(), 0);
                if (pi != null) {
                    version = pi.versionName;
                }
            } catch (Exception e) {
            }
        }

        textView.setText(Html.fromHtml(getString(R.string.about).replace("X.YY", version)));
        return rootView;
    }
}
