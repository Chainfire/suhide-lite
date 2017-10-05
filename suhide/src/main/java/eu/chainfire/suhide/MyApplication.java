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

import android.app.Application;
import android.os.Handler;

import java.util.ArrayList;
import java.util.List;

import eu.chainfire.libsuperuser.Debug;
import eu.chainfire.libsuperuser.Shell;

//TODO move startup stuff to its own class

public class MyApplication extends Application {
    public interface OnStartupListener {
        void onStartupProgressMessage(String message);
        void onStartupErrorMessage(String message);
        void onStartupComplete();
    }

    private final Handler handler = new Handler();
    private Thread startup = null;

    private boolean startingUp = false;
    private String startupProgress = null;
    private String startupError = null;
    private boolean startupComplete = false;
    private OnStartupListener startupListener = null;

    private AppAdapter appAdapter = new AppAdapter();

    private Shell.Interactive rootShell = null;

    private List<String> suhideUid = new ArrayList<String>();
    private List<String> suhidePkg = new ArrayList<String>();

    @Override
    public void onCreate() {
        super.onCreate();
        Debug.setDebug(BuildConfig.DEBUG);
        startup();
    }

    public void startup() {
        if (startingUp) return;
        startingUp = true;

        startup = new Thread(new Runnable() {
            @Override
            public void run() {
                asyncStartup();
                startup = null;
            }
        });
        startup.start();
    }

    public void clearStartup() {
        startingUp = false;
        startupProgress = null;
        startupError = null;
        startupComplete = false;
    }

    public AppAdapter getAppAdapter() {
        return appAdapter;
    }

    public Shell.Interactive getRootShell() {
        return rootShell;
    }

    public void setStartupListener(OnStartupListener listener) {
        this.startupListener = listener;
        if (listener != null) {
            if (startupComplete) {
                listener.onStartupComplete();
            } else if (startupError != null) {
                listener.onStartupErrorMessage(startupError);
            } else if (startupProgress != null) {
                listener.onStartupProgressMessage(startupProgress);
            }
        }
    }

    private void setStartupProgress(final String message) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                startupProgress = message;
                if (startupListener != null) {
                    startupListener.onStartupProgressMessage(message);
                }
            }
        });
    }
    
    private void setStartupError(final String message) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                startupError = message;
                if (startupListener != null) {
                    startupListener.onStartupErrorMessage(message);
                }
            }
        });
    }

    private void setStartupComplete() {
        handler.post(new Runnable() {
            @Override
            public void run() {
                startupComplete = true;
                if (startupListener != null) {
                    startupListener.onStartupComplete();
                }
            }
        });
    }

    private void asyncStartup() {
        setStartupProgress(getString(R.string.startup_loading));

        setStartupProgress(getString(R.string.startup_getting_root));
        final boolean[] suGranted = { false };
        rootShell = (new Shell.Builder())
                .useSU()
                .addCommand("id", 0, new Shell.OnCommandResultListener() {
                    @Override
                    public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                        synchronized (suGranted) {
                            suGranted[0] = true;
                        }
                    }
                })
                .open(new Shell.OnCommandResultListener() {
                    @Override
                    public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                    }
                });
        rootShell.waitForIdle();
        if (!suGranted[0]) {
            setStartupError(getString(R.string.error_no_root));
            return;
        }

        setStartupProgress(getString(R.string.startup_detecting_root_state));
        final boolean[] isSuperSU = { false };
        rootShell.addCommand("su -v", 0, new Shell.OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                for (String line : output) {
                    if (line.toLowerCase().contains("supersu")) {
                        isSuperSU[0] = true;
                    }
                }
            }
        });
        rootShell.waitForIdle();
        if (!isSuperSU[0]) {
            setStartupError(getString(R.string.error_no_supersu));
            return;
        }

        final boolean[] haveLink = { false };
        rootShell.addCommand("readlink /sbin/supersu_link", 0, new Shell.OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                for (String line : output) {
                    if (line.contains("/data")) {
                        haveLink[0] = true;
                    }
                }
            }
        });
        rootShell.waitForIdle();
        if (!haveLink[0]) {
            setStartupError(getString(R.string.error_no_sbin_bind));
            return;
        }

        setStartupProgress(getString(R.string.startup_reading_config));
        final boolean[] haveSuHide = { false };
        rootShell.addCommand("ls -d /sbin/supersu/suhide", 0, new Shell.OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                for (String line : output) {
                    if (line.contains("/suhide")) {
                        haveSuHide[0] = true;
                    }
                }
            }
        });
        rootShell.waitForIdle();
        if (!haveSuHide[0]) {
            setStartupError(getString(R.string.error_no_suhide));
            return;
        }

        suhideUid.clear();
        rootShell.addCommand("cat /sbin/supersu/suhide/suhide.uid", 0, new Shell.OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                for (String line : output) {
                    line = line.trim();
                    if (line.length() > 0) {
                        suhideUid.add(line);
                    }
                }
            }
        });
        suhidePkg.clear();
        rootShell.addCommand("cat /sbin/supersu/suhide/suhide.pkg", 0, new Shell.OnCommandResultListener() {
            @Override
            public void onCommandResult(int commandCode, int exitCode, List<String> output) {
                for (String line : output) {
                    line = line.trim();
                    if (line.length() > 0) {
                        suhidePkg.add(line);
                    }
                }
            }
        });

        setStartupProgress(getString(R.string.startup_detecting_apps));
        appAdapter.loadAppItems(this);

        for (int i = suhideUid.size() - 1; i >= 0; i--) {
            boolean found = false;
            for (int j = 0; j < appAdapter.getItemCount(); j++) {
                String uid = suhideUid.get(i);
                AppAdapter.AppItem appItem = appAdapter.getItem(j);
                if (uid.equals(appItem.packageName) || uid.equals(String.valueOf(appItem.uid))) {
                    appAdapter.getItem(j).setState(AppAdapter.AppItem.NO_ROOT);
                    found = true;
                }
            }
            if (found) suhideUid.remove(i);
        }
        for (int i = suhidePkg.size() - 1; i >= 0; i--) {
            for (int j = 0; j < appAdapter.getItemCount(); j++) {
                if (suhidePkg.get(i).equals(appAdapter.getItem(j).packageName)) {
                    appAdapter.getItem(j).setState(AppAdapter.AppItem.HIDDEN);
                    suhidePkg.remove(i);
                    break;
                }
            }
        }

        setStartupProgress(getString(R.string.startup_complete));
        setStartupComplete();
    }

    public void save() {
        List<String> commands = new ArrayList<String>();
        commands.add("rm /sbin/supersu/suhide/suhide.uid");
        commands.add("rm /sbin/supersu/suhide/suhide.pkg");

        List<String> listed = new ArrayList<String>();

        for (String line : suhideUid) {
            // these are actual uids or partial names, this GUI doesn't handle those,
            // but we do save them if they were present during load
            listed.add(line);
            commands.add("echo \"" + line + "\">>/sbin/supersu/suhide/suhide.uid");
        }
        for (String line : suhidePkg) {
            commands.add("echo \"" + line + "\">>/sbin/supersu/suhide/suhide.pkg");
        }

        for (int i = 0; i < appAdapter.getItemCount(); i++) {
            AppAdapter.AppItem appItem = appAdapter.getItem(i);
            if (appItem.state == AppAdapter.AppItem.NO_ROOT) {
                String s = String.valueOf(appItem.uid);
                if (!listed.contains(s)) {
                    commands.add("echo \"" + s + "\">>/sbin/supersu/suhide/suhide.uid");
                    listed.add(s);
                }
            } else if (appItem.state == AppAdapter.AppItem.HIDDEN) {
                commands.add("echo \"" + appItem.packageName + "\">>/sbin/supersu/suhide/suhide.pkg");
            }
        }

        commands.add("chmod 0600 /sbin/supersu/suhide/suhide.uid");
        commands.add("chmod 0600 /sbin/supersu/suhide/suhide.pkg");
        commands.add("chcon u:object_r:system_file:s0 /sbin/supersu/suhide/suhide.uid");
        commands.add("chcon u:object_r:system_file:s0 /sbin/supersu/suhide/suhide.pkg");

        rootShell.addCommand(commands);
    }

    public void kill(boolean wait) {
        rootShell.addCommand(new String[] {
                "for PID in `ls /proc`; do\n" +
                "  if [ -f \"/proc/$PID/cmdline\" ]; then\n" +
                "    CMDLINE=$(cat /proc/$PID/cmdline | tr -d '\\000')\n" +
                "    UID=$(cat /proc/$PID/status | grep -i uid)\n" +
                "    if [ ! -z \"$CMDLINE\" ]; then\n" +
                "      for ENTRY in `cat /sbin/supersu/suhide/suhide.uid`; do\n" +
                "        if (`echo \"$ENTRY\" | grep \"\\.\" >/dev/null`); then\n" +
                "          if (`echo \"$CMDLINE\" | grep \"^$ENTRY\" >/dev/null`); then\n" +
                "            if (`echo \"$CMDLINE\" | grep \"^$ENTRY$\" >/dev/null`) || (`echo \"$CMDLINE\" | grep \"^$ENTRY:\" >/dev/null`); then\n" +
                "              echo KILLING PACKAGE $PID $ENTRY\n" +
                "              kill -9 $PID\n" +
                "            fi\n" +
                "          fi\n" +
                "        else\n" +
                "          if (`echo \"$UID\" | grep \"$ENTRY\" >/dev/null`); then\n" +
                "            echo KILLING UID $PID $ENTRY\n" +
                "            kill -9 $PID\n" +
                "          fi\n" +
                "        fi\n" +
                "      done\n" +
                "    fi\n" +
                "  fi\n" +
                "done\n"
        });

        if (wait) {
            rootShell.waitForIdle();
        }
    }
}
